#include "ProtocolTestClient.h"

#include <QByteArray>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <stdexcept>

namespace msa::infra::test {
namespace {
constexpr int kStopTimeoutMs{3000};
constexpr int kHttpTimeoutMs{8000};
constexpr int kContextBudget{50000};
constexpr int kGenerationLimit{128};
constexpr int kDeadlineIntervalMs{50};

[[nodiscard]] QJsonObject result(const QJsonObject& response)
{
    if (!response.value("ok").toBool()) {
        throw std::runtime_error{"Неуспешный ответ тестового сервера: "
            + QJsonDocument{response}.toJson(QJsonDocument::Compact).toStdString()};
    }

    return response.value("result").toObject();
}

struct HttpRequestBuffer {
    QByteArray bytes{};
    bool complete{false};
};

class LocalModelServer {
public:
    LocalModelServer();
    [[nodiscard]] quint16 port() const;
    [[nodiscard]] int requestCount() const;
    void reply(bool success);

private:
    void accept();
    QTcpServer m_listener{};
    std::deque<QPointer<QTcpSocket>> m_pending{};
    int m_requestCount{0};
};

LocalModelServer::LocalModelServer()
{
    if (!m_listener.listen(QHostAddress::LocalHost, 0)) {
        throw std::runtime_error{"Не удалось запустить локальную модель теста"};
    }
    QObject::connect(&m_listener, &QTcpServer::newConnection, &m_listener, [this]() { accept(); });
}

quint16 LocalModelServer::port() const
{
    return m_listener.serverPort();
}

int LocalModelServer::requestCount() const
{
    return m_requestCount;
}

void LocalModelServer::accept()
{
    while (m_listener.hasPendingConnections()) {
        auto* socket = m_listener.nextPendingConnection();
        const auto buffer = std::make_shared<HttpRequestBuffer>();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
            buffer->bytes += socket->readAll();
            const auto headerEnd = buffer->bytes.indexOf("\r\n\r\n");
            if (buffer->complete || headerEnd < 0) { return; }
            const auto header = QString::fromLatin1(buffer->bytes.first(headerEnd));
            const QRegularExpression lengthPattern{"(?:^|\\r\\n)Content-Length:\\s*([0-9]+)",
                QRegularExpression::CaseInsensitiveOption};
            const auto length = lengthPattern.match(header);
            if (!length.hasMatch() || buffer->bytes.size() - headerEnd - 4 < length.captured(1).toLongLong()) { return; }
            buffer->complete = true;
            ++m_requestCount;
            m_pending.emplace_back(socket);
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void LocalModelServer::reply(bool success)
{
    if (m_pending.empty() || !m_pending.front()) {
        throw std::runtime_error{"Нет ожидающего HTTP-запроса тестовой модели"};
    }
    auto* socket = m_pending.front().data();
    m_pending.pop_front();
    QByteArray body{"{}"};
    QByteArray status{"503 Service Unavailable"};
    if (success) {
        const QJsonObject content{{"reply", "Ответ после восстановления связи"}, {"tone", "Neutral"},
            {"mood_recording", QJsonValue::Null}, {"profile_facts", QJsonArray{}}};
        const QJsonObject envelope{{"choices", QJsonArray{QJsonObject{{"finish_reason", "stop"},
            {"message", QJsonObject{{"role", "assistant"},
                {"content", QString::fromUtf8(QJsonDocument{content}.toJson(QJsonDocument::Compact))}}}}}}};
        body = QJsonDocument{envelope}.toJson(QJsonDocument::Compact);
        status = "200 OK";
    }
    socket->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: "
        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}

class ServerProcessTest : public ::testing::Test {
protected:
    ~ServerProcessTest() override;
    void SetUp() override;
    void startServer();
    void stopServer();
    [[nodiscard]] QJsonObject settings() const;
    [[nodiscard]] QJsonArray history(ProtocolTestClient& client, const QJsonValue& chatId);
    [[nodiscard]] QJsonValue createChat();
    void reconnect();

    QTemporaryDir m_directory{};
    LocalModelServer m_model{};
    QProcess m_process{};
    ProtocolTestClient m_client{};
    QJsonObject m_auth{};
    quint16 m_port{0};
};

ServerProcessTest::~ServerProcessTest()
{
    stopServer();
}

QJsonObject ServerProcessTest::settings() const
{
    const auto endpoint = QString{"http://127.0.0.1:%1"}.arg(m_model.port());

    return {{"listen_address", "127.0.0.1"}, {"port", 0}, {"token", "process-test-token"},
        {"time_zone", "Etc/UTC"}, {"database_path", "assistant.sqlite"},
        {"context_max_chars", kContextBudget}, {"check_interval_ms", kDeadlineIntervalMs},
        {"llama", QJsonObject{{"completion_url", endpoint + "/v1/chat/completions"}, {"model", "test-model"},
            {"api_key", ""}, {"timeout_ms", kHttpTimeoutMs}, {"max_response_tokens", kGenerationLimit}}},
        {"services", QJsonObject{{"timeout_ms", kHttpTimeoutMs}, {"user_agent", "MSA-local-tests"},
            {"nominatim", endpoint + "/search"}, {"osrm_foot", endpoint + "/route/v1/foot"},
            {"osrm_bike", endpoint + "/route/v1/bike"}, {"osrm_driving", endpoint + "/route/v1/driving"},
            {"weather", endpoint + "/forecast"}, {"search", endpoint + "/search"}}}};
}

void ServerProcessTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
    QFile file{m_directory.filePath("server.json")};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    const auto bytes = QJsonDocument{settings()}.toJson();
    ASSERT_EQ(file.write(bytes), bytes.size());
    file.close();
    startServer();
    reconnect();
}

void ServerProcessTest::startServer()
{
    m_port = 0;
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_FORCE_STDERR_LOGGING", "1");
    m_process.setProcessEnvironment(environment);
    m_process.setProgram(QString::fromUtf8(MSA_SERVER_EXECUTABLE));
    m_process.setArguments({"--config", m_directory.filePath("server.json")});
    m_process.setWorkingDirectory(m_directory.path());
    m_process.start();
    QByteArray output;
    const auto started = waitForTest([this, &output]() {
        if (m_port != 0) { return true; }
        output += m_process.readAll();
        const QRegularExpression portPattern{"TCP-[^\\r\\n]*: ([0-9]+)"};
        const auto match = portPattern.match(QString::fromLatin1(output));
        if (!match.hasMatch()) { return false; }
        m_port = match.captured(1).toUShort();
        return m_port != 0;
    });
    if (!started) {
        throw std::runtime_error{QString{"msa_server не сообщил порт: state=%1, exit=%2, error=%3, output=%4"}
            .arg(static_cast<int>(m_process.state())).arg(m_process.exitCode())
            .arg(m_process.errorString(), QString::fromUtf8(output)).toStdString()};
    }
}

void ServerProcessTest::stopServer()
{
    if (m_process.state() == QProcess::NotRunning) { return; }
    m_process.kill();
    static_cast<void>(m_process.waitForFinished(kStopTimeoutMs));
}

void ServerProcessTest::reconnect()
{
    m_client.connectTo(m_port);
    m_auth = result(m_client.authenticate());
}

QJsonArray ServerProcessTest::history(ProtocolTestClient& client, const QJsonValue& chatId)
{
    return result(client.request("chat.messages", {{"chat_id", chatId},
        {"after_message_id", QJsonValue::Null}, {"limit", 100}})).value("messages").toArray();
}

QJsonValue ServerProcessTest::createChat()
{
    return result(m_client.request("chat.create")).value("chat_id");
}

TEST_F(ServerProcessTest, LostCommandResponseIsRecoveredByReadingWithoutReplayingAndSurvivesRestart)
{
    const auto initial = result(m_client.request("get_all"));
    const auto mainChatId = initial.value("main_chat_id");
    const auto chatId = createChat();
    ProtocolTestClient observer;
    observer.connectTo(m_port);
    static_cast<void>(result(observer.authenticate()));
    m_client.sendRequest("lost-command", "command.execute", {{"chat_id", chatId},
        {"text", "список дел: 1. Купить молоко 2. Прочитать книгу"}, {"reference", QJsonValue::Null}});
    // Независимый наблюдатель доказывает commit; исходный клиент так и не читает подтверждение.
    ASSERT_TRUE(waitForTest([this, &observer, &chatId]() { return history(observer, chatId).size() == 2; }));
    m_client.disconnect();
    reconnect();
    EXPECT_EQ(result(m_client.request("get_all")).value("tasks").toArray().size(), 2);
    const auto messages = history(m_client, chatId);
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages.first().toObject().value("role"), "User");
    EXPECT_EQ(messages.last().toObject().value("role"), "Assistant");
    // ID можно использовать в новой сессии, но это не повод повторять запись.
    EXPECT_TRUE(m_client.request("ping", {}, "lost-command").value("ok").toBool());
    const auto instance = m_auth.value("server_instance_id");
    m_client.disconnect();
    observer.disconnect();
    stopServer();
    startServer();
    reconnect();
    EXPECT_NE(m_auth.value("server_instance_id"), instance);
    const auto restored = result(m_client.request("get_all"));
    EXPECT_EQ(restored.value("main_chat_id"), mainChatId);
    EXPECT_EQ(restored.value("tasks").toArray().size(), 2);
    EXPECT_EQ(history(m_client, chatId), messages);
    EXPECT_EQ(m_model.requestCount(), 0);
}

TEST_F(ServerProcessTest, DisconnectedGenerationFinishesInOriginalChatWithoutAnsweringNewSession)
{
    const auto chatId = createChat();
    m_client.sendRequest("operation", "chat.send", {{"chat_id", chatId},
        {"text", "Полный вопрос до обрыва 🙂"}, {"search_query", QJsonValue::Null}});
    ASSERT_TRUE(waitForTest([this]() { return m_model.requestCount() == 1; }));
    m_client.disconnect();
    reconnect();
    EXPECT_TRUE(m_client.request("ping", {}, "operation").value("ok").toBool());
    const auto pending = history(m_client, chatId);
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending.first().toObject().value("role"), "User");
    const auto refused = m_client.request("chat.retry", {{"chat_id", chatId},
        {"expected_user_message_id", pending.first().toObject().value("id")}, {"search_query", QJsonValue::Null}});
    EXPECT_EQ(refused.value("error").toObject().value("code"), "busy");
    EXPECT_EQ(refused.value("error").toObject().value("effect"), "none");
    m_model.reply(true);
    QJsonArray completed;
    ASSERT_TRUE(waitForTest([this, &chatId, &completed]() {
        completed = history(m_client, chatId);
        return completed.size() == 2;
    }));
    EXPECT_EQ(completed.first(), pending.first());
    EXPECT_EQ(completed.last().toObject().value("chat_id"), chatId);
    EXPECT_EQ(completed.last().toObject().value("text"), "Ответ после восстановления связи");
    EXPECT_EQ(m_model.requestCount(), 1);
    const auto& frames = m_client.frames();
    EXPECT_EQ(std::ranges::count_if(frames, [](const auto& frame) {
        return frame.value("type") == "response" && frame.value("request_id") == "operation";
    }), 1);
    EXPECT_TRUE(result(m_client.request("get_all")).value("tasks").toArray().isEmpty());
}

TEST_F(ServerProcessTest, ExplicitRetryAfterReconnectUsesSavedUserAndNeverRepeatsModelAutomatically)
{
    const auto chatId = createChat();
    m_client.sendRequest("operation", "chat.send", {{"chat_id", chatId},
        {"text", "Запрос с ошибкой модели"}, {"search_query", QJsonValue::Null}});
    ASSERT_TRUE(waitForTest([this]() { return m_model.requestCount() == 1; }));
    m_model.reply(false);
    const auto failure = m_client.response("operation").value("error").toObject();
    EXPECT_EQ(failure.value("code"), "llm_unavailable");
    EXPECT_EQ(failure.value("effect"), "user_saved");
    const auto userId = failure.value("user_message_id");
    ASSERT_TRUE(userId.isString());
    m_client.disconnect();
    reconnect();
    const auto pending = history(m_client, chatId);
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending.first().toObject().value("id"), userId);
    static_cast<void>(result(m_client.request("get_all")));
    EXPECT_EQ(m_model.requestCount(), 1);
    const auto conflict = m_client.request("chat.retry", {{"chat_id", chatId},
        {"expected_user_message_id", "999999"}, {"search_query", QJsonValue::Null}});
    EXPECT_EQ(conflict.value("error").toObject().value("code"), "conflict");
    EXPECT_EQ(m_model.requestCount(), 1);
    m_client.sendRequest("operation", "chat.retry", {{"chat_id", chatId},
        {"expected_user_message_id", userId}, {"search_query", QJsonValue::Null}});
    ASSERT_TRUE(waitForTest([this]() { return m_model.requestCount() == 2; }));
    m_model.reply(true);
    const auto reply = result(m_client.response("operation"));
    EXPECT_EQ(reply.value("user_message_id"), userId);
    const auto completed = history(m_client, chatId);
    ASSERT_EQ(completed.size(), 2);
    EXPECT_EQ(completed.first(), pending.first());
    const auto duplicate = m_client.request("chat.retry", {{"chat_id", chatId},
        {"expected_user_message_id", userId}, {"search_query", QJsonValue::Null}});
    EXPECT_EQ(duplicate.value("error").toObject().value("code"), "conflict");
    EXPECT_EQ(m_model.requestCount(), 2);
}

TEST_F(ServerProcessTest, AuthErrorsPartialEofFragmentedUtf8AndDuplicateIdsDoNotDuplicateWrites)
{
    ProtocolTestClient unauthenticated;
    unauthenticated.connectTo(m_port);
    const auto refused = unauthenticated.authenticate("incorrect-token");
    EXPECT_EQ(refused.value("error").toObject().value("code"), "unauthorized");
    EXPECT_TRUE(unauthenticated.waitDisconnected());
    const QJsonObject taskParams{{"title", "Полный текст 🙂"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}};
    const QJsonObject request{{"type", "request"}, {"request_id", "task"}, {"method", "task.create"}, {"params", taskParams}};
    const auto bytes = QJsonDocument{request}.toJson(QJsonDocument::Compact) + '\n';
    const auto split = bytes.indexOf(QByteArray::fromHex("f09f9982")) + 1;
    ASSERT_GT(split, 0);
    m_client.sendBytes(bytes.first(split));
    m_client.disconnect();
    reconnect();
    EXPECT_TRUE(result(m_client.request("get_all")).value("tasks").toArray().isEmpty());
    m_client.sendBytes(bytes.first(split));
    ProtocolTestClient observer;
    observer.connectTo(m_port);
    static_cast<void>(result(observer.authenticate()));
    EXPECT_TRUE(result(observer.request("get_all")).value("tasks").toArray().isEmpty());
    m_client.sendBytes(bytes.sliced(split));
    const auto taskId = result(m_client.response("task")).value("task_id");
    ASSERT_TRUE(taskId.isString());
    const auto duplicate = m_client.request("task.create", taskParams, "task");
    EXPECT_EQ(duplicate.value("error").toObject().value("code"), "invalid_request");
    EXPECT_EQ(duplicate.value("error").toObject().value("effect"), "none");
    EXPECT_EQ(result(m_client.request("get_all")).value("tasks").toArray().size(), 1);
    EXPECT_EQ(m_client.request("unknown.method").value("error").toObject().value("code"), "unknown_method");
    EXPECT_TRUE(m_client.request("ping").value("ok").toBool());
    static_cast<void>(result(m_client.request("task.set_done", {{"task_id", taskId}, {"done", true}})));
    EXPECT_TRUE(result(observer.request("get_all")).value("tasks").toArray().first().toObject().value("done").toBool());
}
} // namespace
} // namespace msa::infra::test

