#include "NetworkServices.h"
#include "ServerRuntime.h"
#include "ServerSettings.h"

#include "../core/FakeClock.h"
#include "msa/infra/ProtocolPayload.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/TcpClient.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace msa::server {
namespace {
constexpr int kWaitMs{4000};
constexpr int kSliceMs{5};
constexpr unsigned long kPollMs{1};
constexpr auto kDay = std::chrono::year{2026} / 11 / 9;
constexpr auto kNoon = std::chrono::local_days{kDay} + std::chrono::hours{12};
constexpr std::size_t kTestBudget{50000};
constexpr int kTestIntervalMs{10};
constexpr int kHttpDelayMs{120};

bool waitUntil(const std::function<bool()>& predicate)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < kWaitMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kSliceMs);
        QThread::msleep(kPollMs);
    }

    return predicate();
}

QJsonObject receive(QTcpSocket& socket, const QString& requestId, std::vector<QJsonObject>* events = nullptr)
{
    QJsonObject response;
    const auto received = waitUntil([&socket, &requestId, &response, events]() {
        if (!response.isEmpty()) { return true; }
        while (socket.canReadLine()) {
            const auto frame = QJsonDocument::fromJson(socket.readLine()).object();
            if (frame.value("type") == "event" && events) { events->push_back(frame); }
            if (frame.value("request_id").toString() == requestId) { response = frame; return true; }
        }
        return false;
    });
    if (!received) { ADD_FAILURE() << "No response for " << requestId.toStdString(); }

    return response;
}

void send(QTcpSocket& socket, const infra::ProtocolMessage& message)
{
    const auto encoded = infra::ProtocolCodec::encode(message);
    ASSERT_TRUE(std::holds_alternative<QByteArray>(encoded));
    const auto& bytes = std::get<QByteArray>(encoded);
    ASSERT_EQ(socket.write(bytes), bytes.size());
    socket.flush();
}

void connectClient(QTcpSocket& client, quint16 port)
{
    client.connectToHost(QHostAddress::LocalHost, port);
    ASSERT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::ConnectedState; }));
    send(client, infra::ProtocolAuth{"auth", infra::kProtocolVersion, "test-token"});
    ASSERT_TRUE(receive(client, "auth").value("ok").toBool());
}

ServerSettings settings(const QString& path)
{
    ServerSettings result;
    result.listenAddress = QHostAddress::LocalHost;
    result.token = "test-token";
    result.timeZone = "Etc/UTC";
    result.databasePath = path;
    result.contextMaxChars = kTestBudget;
    result.checkIntervalMs = kTestIntervalMs;
    result.llama = {QUrl{"http://127.0.0.1/v1/chat/completions"}, "test-model", {}, std::chrono::milliseconds{kWaitMs}, 128};
    result.http = {std::chrono::milliseconds{kWaitMs}, "MSA-tests"};
    result.nominatim = QUrl{"http://127.0.0.1/search"};
    result.osrm = {QUrl{"http://127.0.0.1/foot"}, QUrl{"http://127.0.0.1/bike"}, QUrl{"http://127.0.0.1/driving"}};
    result.weather = QUrl{"http://127.0.0.1/forecast"};
    result.search = QUrl{"http://127.0.0.1/search"};

    return result;
}

class TestExternal final : public core::LlmClient, public core::Geocoder, public core::RoutePlanner,
    public core::WeatherProvider, public core::WebSearchProvider {
public:
    core::LlmResult next{core::LlmResponse{.reply = "Ответ модели", .tone = core::MoodScore::Positive}};
    std::vector<core::LlmRequest> requests{};

protected:
    core::LlmResult requestCompletion(const core::LlmRequest& request) override
    {
        requests.push_back(request);
        return next;
    }
    core::ExternalServiceResult<std::vector<core::GeocodedPlace>> requestPlaces(const std::string&) override
    {
        return std::vector<core::GeocodedPlace>{{{0, 0}, "Первый адрес"}, {{0.001, 0.001}, "Второй адрес"}};
    }
    core::ExternalServiceResult<core::RoutePlan> requestRoute(
        core::GeoCoordinates, core::GeoCoordinates, core::TravelMode) override
    {
        return core::RoutePlan{.distanceMeters = 100, .durationSeconds = 50,
            .steps = {{.type = "depart", .distanceMeters = 100}, {.type = "arrive"}}};
    }
    core::ExternalServiceResult<core::WeatherForecast> requestForecast(core::GeoCoordinates) override
    {
        return core::WeatherForecast{.temperatureCelsius = 10, .apparentTemperatureCelsius = 8, .weatherCode = 0};
    }
    core::ExternalServiceResult<std::vector<core::WebSearchResult>> requestSearch(const std::string&) override
    {
        return std::vector<core::WebSearchResult>{{"Материал", "https://example.org/article", "Описание"}};
    }
};

class ServerRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QJsonObject call(const QString& method, const QJsonObject& params = {});
    [[nodiscard]] QJsonObject result(const QString& method, const QJsonObject& params = {});
    [[nodiscard]] QJsonObject messages(const QString& chatId);
    [[nodiscard]] QJsonObject command(const QString& commandText, const QString& chatId);
    [[nodiscard]] QString mainChat();
    void sql(const QString& statement);

    QTemporaryDir m_directory{};
    core::FakeClock m_clock{kNoon};
    TestExternal m_external{};
    ServerSettings m_settings{settings(m_directory.filePath("assistant.sqlite"))};
    std::unique_ptr<ServerRuntime> m_server{};
    QTcpSocket m_client{};
    int m_requestNumber{0};
};

void ServerRuntimeTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
    m_server = std::make_unique<ServerRuntime>(m_settings, m_clock,
        ExternalServices{m_external, m_external, m_external, m_external, m_external, "test-source"});
    ASSERT_TRUE(m_server->start());
    connectClient(m_client, m_server->port());
}

QJsonObject ServerRuntimeTest::call(const QString& method, const QJsonObject& params)
{
    const auto requestId = QString::number(++m_requestNumber);
    send(m_client, infra::ProtocolRequest{requestId, method, params});

    return receive(m_client, requestId);
}

QJsonObject ServerRuntimeTest::result(const QString& method, const QJsonObject& params)
{
    const auto response = call(method, params);
    EXPECT_TRUE(response.value("ok").toBool()) << QJsonDocument(response).toJson().toStdString();

    return response.value("result").toObject();
}

QString ServerRuntimeTest::mainChat()
{
    return result("get_all").value("main_chat_id").toString();
}

QJsonObject ServerRuntimeTest::messages(const QString& chatId)
{
    return result("chat.messages", {{"chat_id", chatId}, {"after_message_id", QJsonValue::Null}, {"limit", 100}});
}

QJsonObject ServerRuntimeTest::command(const QString& commandText, const QString& chatId)
{
    return call("command.execute", {{"chat_id", chatId}, {"text", commandText}, {"reference", QJsonValue::Null}});
}

void ServerRuntimeTest::sql(const QString& statement)
{
    infra::SqliteDatabase database{m_settings.databasePath, infra::sqliteMigrations()};
    database.execute(statement);
}

TEST_F(ServerRuntimeTest, RestoresMainIdentityAndDoesNotPersistPerRequestChatSelection)
{
    const auto mainId = mainChat();
    const auto otherId = result("chat.create").value("chat_id").toString();
    ASSERT_NE(mainId, otherId);
    EXPECT_TRUE(command("добавь задачу купить молоко", otherId).value("ok").toBool());
    EXPECT_EQ(messages(otherId).value("messages").toArray().size(), 2);
    EXPECT_EQ(messages(mainId).value("messages").toArray().size(), 1);
    m_client.abort();
    m_server.reset();
    m_server = std::make_unique<ServerRuntime>(m_settings, m_clock,
        ExternalServices{m_external, m_external, m_external, m_external, m_external, "test-source"});
    ASSERT_TRUE(m_server->start());
    connectClient(m_client, m_server->port());
    EXPECT_EQ(mainChat(), mainId);
    EXPECT_EQ(messages(mainId).value("messages").toArray().size(), 1);
    EXPECT_EQ(messages(otherId).value("messages").toArray().size(), 2);
    infra::SqliteDatabase database{m_settings.databasePath, infra::sqliteMigrations()};
    infra::SqliteChatRepository chats{database};
    EXPECT_FALSE(chats.currentChatId());
}

TEST_F(ServerRuntimeTest, ProductionClientAuthenticatesWritesAndRefreshesSnapshotAfterManualReconnect)
{
    infra::TcpClient client{{"127.0.0.1", m_server->port(), "test-token", std::chrono::milliseconds{kWaitMs},
        std::chrono::milliseconds{kWaitMs}, std::chrono::milliseconds{5}, std::chrono::milliseconds{20}}};
    int readyCount{0};
    std::vector<infra::ProtocolSuccess> replies;
    QObject::connect(&client, &infra::TcpClient::ready, &client,
        [&readyCount](infra::TcpClientServerInfo, QJsonObject) { ++readyCount; });
    QObject::connect(&client, &infra::TcpClient::responseReceived, &client,
        [&replies](infra::ProtocolSuccess reply) { replies.push_back(std::move(reply)); });
    client.connectToServer();
    ASSERT_TRUE(waitUntil([&client, &readyCount]() { return client.state() == infra::TcpClientState::Ready && readyCount == 1; }));
    EXPECT_TRUE(client.initialSnapshot().value("tasks").toArray().isEmpty());
    const auto requestId = client.sendRequest("task.create", {{"title", "Через настоящий клиент"},
        {"due_date", QJsonValue::Null}, {"priority", "Normal"}});
    ASSERT_TRUE(waitUntil([&replies]() { return replies.size() == 1; }));
    EXPECT_EQ(replies.front().requestId, requestId);
    EXPECT_TRUE(replies.front().result.value("task_id").isString());
    client.reconnect();
    ASSERT_TRUE(waitUntil([&client, &readyCount]() { return client.state() == infra::TcpClientState::Ready && readyCount == 2; }));
    EXPECT_EQ(client.initialSnapshot().value("tasks").toArray().size(), 1);
}

TEST_F(ServerRuntimeTest, PlannerMethodsPersistAndCalendarReturnsOccurrences)
{
    const auto taskId = result("task.create", {{"title", "Задача"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}})
        .value("task_id").toString();
    static_cast<void>(result("task.update", {{"task_id", taskId}, {"title", "Изменена"}, {"due_date", "2026-11-10"}, {"priority", "High"}}));
    static_cast<void>(result("task.set_done", {{"task_id", taskId}, {"done", true}}));
    const auto eventId = result("event.create", {{"title", "Встреча"}, {"date", "2026-11-09"}, {"time", "15:00"},
        {"description", "Текст"}, {"repeat", "Daily"}}).value("event_id").toString();
    static_cast<void>(result("event.update", {{"event_id", eventId}, {"title", "Обновлена"}, {"date", "2026-11-09"},
        {"time", QJsonValue::Null}, {"description", ""}, {"repeat", "Daily"}}));
    EXPECT_EQ(result("calendar.day", {{"date", "2026-11-10"}}).value("events").toArray().size(), 1);
    EXPECT_EQ(result("calendar.month", {{"year", 2026}, {"month", 11}}).value("days").toArray().first().toInt(), 9);
    const auto habitId = result("habit.create", {{"name", "Чтение"}, {"target", 3.5}, {"unit", "стр"}, {"period", "Week"}})
        .value("habit_id").toString();
    static_cast<void>(result("habit.log", {{"habit_id", habitId}, {"value", 1.5}}));
    const auto snapshot = result("get_all");
    EXPECT_TRUE(snapshot.value("tasks").toArray().first().toObject().value("done").toBool());
    EXPECT_DOUBLE_EQ(snapshot.value("habits").toArray().first().toObject().value("progress").toDouble(), 1.5);
    static_cast<void>(result("task.delete", {{"task_id", taskId}}));
    static_cast<void>(result("event.delete", {{"event_id", eventId}}));
    static_cast<void>(result("habit.delete", {{"habit_id", habitId}}));
    EXPECT_TRUE(result("get_all").value("tasks").toArray().isEmpty());
    EXPECT_EQ(call("task.delete", {{"task_id", taskId}}).value("error").toObject().value("code"), "not_found");
}

TEST_F(ServerRuntimeTest, CommandsCommitListAndHistoryTogetherAndRollbackOnSecondTaskOrAssistantFailure)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    static_cast<void>(result("task.create", {{"title", "Старая задача"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}}));
    sql("CREATE TRIGGER fail_task BEFORE INSERT ON tasks WHEN NEW.title = 'Вторая задача' BEGIN SELECT RAISE(ABORT, 'SECRET-SQL'); END");
    const auto failed = command("список дел: 1. Первая задача 2. Вторая задача", chatId);
    EXPECT_FALSE(failed.value("ok").toBool());
    EXPECT_FALSE(QJsonDocument(failed).toJson().contains("SECRET"));
    EXPECT_EQ(result("get_all").value("tasks").toArray().size(), 1);
    EXPECT_TRUE(messages(chatId).value("messages").toArray().isEmpty());
    sql("DROP TRIGGER fail_task");
    sql("CREATE TRIGGER fail_assistant BEFORE INSERT ON chat_messages WHEN NEW.role = 'Assistant' BEGIN SELECT RAISE(ABORT, 'SECRET'); END");
    EXPECT_FALSE(command("добавь задачу третья задача", chatId).value("ok").toBool());
    EXPECT_EQ(result("get_all").value("tasks").toArray().size(), 1);
    EXPECT_TRUE(messages(chatId).value("messages").toArray().isEmpty());
    sql("DROP TRIGGER fail_assistant");
    const auto success = command("список дел: 1. Первая задача 2. Вторая задача", chatId);
    EXPECT_TRUE(success.value("ok").toBool());
    EXPECT_EQ(success.value("result").toObject().value("created_ids").toArray().size(), 2);
    EXPECT_EQ(messages(chatId).value("messages").toArray().size(), 2);
}

TEST_F(ServerRuntimeTest, InvalidOrUnrecognizedCommandDoesNotWriteHistoryOrCallModel)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    EXPECT_FALSE(command("Как прошёл день?", chatId).value("result").toObject().value("handled").toBool());
    EXPECT_FALSE(command("список дел: 1. Купить молоко 2. Я 3. Прочитать книгу", chatId)
        .value("result").toObject().value("handled").toBool());
    EXPECT_FALSE(command("добавь задачи: 1. Купить молоко 2. Я 3. Прочитать книгу", chatId)
        .value("result").toObject().value("handled").toBool());
    const auto failed = command("напомни сегодня в 10:00 купить молоко", chatId);
    EXPECT_FALSE(failed.value("ok").toBool());
    EXPECT_EQ(failed.value("error").toObject().value("effect"), "none");
    EXPECT_TRUE(messages(chatId).value("messages").toArray().isEmpty());
    EXPECT_TRUE(result("get_all").value("tasks").toArray().isEmpty());
    EXPECT_TRUE(m_external.requests.empty());
}

TEST_F(ServerRuntimeTest, ChatFailureReturnsSavedUserAndExplicitRetryNeverDuplicatesIt)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    m_external.next = core::LlmFailure{core::LlmFailureKind::Unavailable, "Модель недоступна"};
    const auto failed = call("chat.send", {{"chat_id", chatId}, {"text", "  Полный текст\n🙂  "}, {"search_query", QJsonValue::Null}});
    const auto error = failed.value("error").toObject();
    EXPECT_EQ(error.value("code"), "llm_unavailable");
    EXPECT_EQ(error.value("effect"), "user_saved");
    ASSERT_TRUE(error.value("user_message_id").isString());
    EXPECT_EQ(messages(chatId).value("messages").toArray().size(), 1);
    m_external.next = core::LlmResponse{.reply = "Сохранённый ответ"};
    EXPECT_EQ(call("chat.retry", {{"chat_id", chatId}, {"expected_user_message_id", "99999"}, {"search_query", QJsonValue::Null}})
        .value("error").toObject().value("code"), "conflict");
    const auto reply = result("chat.retry", {{"chat_id", chatId}, {"expected_user_message_id", error.value("user_message_id")},
        {"search_query", QJsonValue::Null}});
    EXPECT_EQ(reply.value("user_message_id"), error.value("user_message_id"));
    EXPECT_EQ(messages(chatId).value("messages").toArray().size(), 2);
    EXPECT_EQ(m_external.requests.size(), 2U);
}

TEST_F(ServerRuntimeTest, ContextBudgetRefusalKeepsUserAndDoesNotCallModel)
{
    m_client.abort();
    m_server.reset();
    m_settings.contextMaxChars = 1;
    m_server = std::make_unique<ServerRuntime>(m_settings, m_clock,
        ExternalServices{m_external, m_external, m_external, m_external, m_external, "test-source"});
    ASSERT_TRUE(m_server->start());
    connectClient(m_client, m_server->port());
    const auto chatId = result("chat.create").value("chat_id").toString();
    const auto failed = call("chat.send", {{"chat_id", chatId}, {"text", "Привет"}, {"search_query", QJsonValue::Null}});
    const auto error = failed.value("error").toObject();
    EXPECT_EQ(error.value("code"), "context_budget_exceeded");
    EXPECT_EQ(error.value("effect"), "user_saved");
    EXPECT_EQ(messages(chatId).value("messages").toArray().size(), 1);
    EXPECT_TRUE(m_external.requests.empty());
}

TEST_F(ServerRuntimeTest, ExplicitSearchDiaryAndProfileAreRoutedToTheirServices)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    m_external.next = core::LlmResponse{.reply = "Ответ", .moodRecording = core::MoodRecordingProposal{
        .score = core::MoodScore::Positive, .reason = core::MoodRecordingReason::ExplicitRequest},
        .profileFacts = {{.text = "Любит книги"}}};
    const auto reply = result("chat.send", {{"chat_id", chatId}, {"text", "Запиши хорошее настроение, люблю книги"}, {"search_query", "новые книги"}});
    EXPECT_EQ(reply.value("search_sources").toArray().size(), 1);
    EXPECT_TRUE(reply.value("mood_id").isString());
    const auto profile = result("profile.get").value("facts").toArray();
    ASSERT_EQ(profile.size(), 1);
    EXPECT_EQ(profile.first().toObject().value("source").toObject().value("chat_id"), chatId);
    EXPECT_EQ(result("mood.history", {{"days", 14}}).value("entries").toArray().size(), 1);
    static_cast<void>(result("mood.record", {{"score", "Neutral"}, {"note", "Явная отметка"}}));
    static_cast<void>(result("profile.remember", {{"text", "Ручной факт"}, {"tags", QJsonArray{"тег"}}, {"always_include", true}}));
    EXPECT_EQ(result("profile.get").value("facts").toArray().size(), 2);
}

TEST_F(ServerRuntimeTest, StorageFailureAfterUserReturnsUnknownAndKeepsCommittedUserWithoutPartialAnswer)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    sql("CREATE TRIGGER fail_assistant BEFORE INSERT ON chat_messages WHEN NEW.role = 'Assistant' BEGIN SELECT RAISE(ABORT, 'SECRET'); END");
    const auto failed = call("chat.send", {{"chat_id", chatId}, {"text", "Привет"}, {"search_query", QJsonValue::Null}});
    EXPECT_FALSE(failed.value("ok").toBool());
    const auto error = failed.value("error").toObject();
    EXPECT_EQ(error.value("code"), "storage_error");
    EXPECT_EQ(error.value("effect"), "unknown");
    EXPECT_TRUE(error.value("user_message_id").isNull());
    EXPECT_EQ(messages(chatId).value("messages").toArray().size(), 1);
}

TEST_F(ServerRuntimeTest, LocationProposalBelongsToSessionAndConfirmationDoesNotFollowNewReference)
{
    const auto reminderId = result("reminder.create", {{"kind", "Place"}, {"title", "Купить молоко"}, {"place", "Магазин"}})
        .value("reminder_id").toString();
    const QJsonObject point{{"latitude", 0}, {"longitude", 0}};
    const auto proposal = result("reminder.location.prepare", {{"reminder_id", reminderId}, {"reference", point}});
    QTcpSocket other;
    connectClient(other, m_server->port());
    send(other, infra::ProtocolRequest{"foreign", "reminder.location.confirm", {{"proposal_id", proposal.value("proposal_id")}, {"candidate_index", 0}}});
    EXPECT_EQ(receive(other, "foreign").value("error").toObject().value("code"), "not_found");
    const auto confirmed = result("reminder.location.confirm", {{"proposal_id", proposal.value("proposal_id")}, {"candidate_index", 0}});
    EXPECT_EQ(confirmed.value("coordinates").toObject(), point);
    static_cast<void>(result("location.update", {{"coordinates", point}}));
    EXPECT_TRUE(result("get_all").value("reminders").toArray().isEmpty());
    static_cast<void>(result("reminder.delete", {{"reminder_id", reminderId}}));
}

TEST_F(ServerRuntimeTest, RouteReturnsActualHistoryIdAndCommandRollsBackItsHistoryOnFailure)
{
    const QJsonObject point{{"latitude", 0}, {"longitude", 0}};
    const auto route = result("route.build", {{"origin", point}, {"destination", "Библиотека"}, {"reference", QJsonValue::Null}, {"mode", "Foot"}});
    const auto history = result("route.history").value("recent").toArray();
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history.first().toObject().value("id"), route.value("calculation_id"));
    const auto chatId = result("chat.create").value("chat_id").toString();
    sql("CREATE TRIGGER fail_assistant BEFORE INSERT ON chat_messages WHEN NEW.role = 'Assistant' BEGIN SELECT RAISE(ABORT, 'SECRET'); END");
    const auto failed = call("command.execute", {{"chat_id", chatId}, {"text", "маршрут до библиотеки"}, {"reference", point}});
    EXPECT_FALSE(failed.value("ok").toBool());
    EXPECT_EQ(result("route.history").value("recent").toArray().size(), 1);
    EXPECT_TRUE(messages(chatId).value("messages").toArray().isEmpty());
    const auto weather = result("weather.get", {{"coordinates", point}});
    EXPECT_DOUBLE_EQ(weather.value("temperature_celsius").toDouble(), 10);
}

TEST_F(ServerRuntimeTest, HistoryPaginatesWholeMessagesAndReportsOversizedSingleMessage)
{
    const auto chatId = result("chat.create").value("chat_id").toString();
    {
        infra::SqliteDatabase database{m_settings.databasePath, infra::sqliteMigrations()};
        infra::SqliteChatRepository chats{database};
        core::ChatService chat{m_clock, chats, chatId.toLongLong()};
        static_cast<void>(chat.addMessage(std::string(600000, 'a'), core::MessageRole::User));
        static_cast<void>(chat.addMessage(std::string(600000, 'b'), core::MessageRole::Assistant));
    }
    const auto first = messages(chatId);
    EXPECT_EQ(first.value("messages").toArray().size(), 1);
    EXPECT_TRUE(first.value("has_more").toBool());
    const auto second = result("chat.messages", {{"chat_id", chatId}, {"after_message_id", first.value("next_after_message_id")}, {"limit", 100}});
    EXPECT_EQ(second.value("messages").toArray().size(), 1);
    EXPECT_FALSE(second.value("has_more").toBool());
    {
        infra::SqliteDatabase database{m_settings.databasePath, infra::sqliteMigrations()};
        infra::SqliteChatRepository chats{database};
        core::ChatService chat{m_clock, chats, chatId.toLongLong()};
        static_cast<void>(chat.addMessage(std::string(infra::kProtocolMaxFrameBytes, 'c'), core::MessageRole::User));
    }
    const auto failed = call("chat.messages", {{"chat_id", chatId}, {"after_message_id", second.value("next_after_message_id")}, {"limit", 100}});
    EXPECT_EQ(failed.value("error").toObject().value("code"), "response_too_large");
}

TEST_F(ServerRuntimeTest, EveningCheckInIsPersistedInMainChatOnly)
{
    const auto mainId = mainChat();
    const auto otherId = result("chat.create").value("chat_id").toString();
    m_clock.set(std::chrono::local_days{kDay} + std::chrono::hours{20});
    ASSERT_TRUE(waitUntil([this, &mainId]() { return messages(mainId).value("messages").toArray().size() == 2; }));
    EXPECT_TRUE(messages(otherId).value("messages").toArray().isEmpty());
    infra::SqliteDatabase database{m_settings.databasePath, infra::sqliteMigrations()};
    infra::SqliteEveningCheckInRepository checkIns{database};
    ASSERT_TRUE(checkIns.pendingCheckIn());
    EXPECT_EQ(checkIns.pendingCheckIn()->chatIdentifier, mainId.toLongLong());
}

QJsonObject settingsObject()
{
    return {{"listen_address", "127.0.0.1"}, {"port", 0}, {"token", "test-token"}, {"time_zone", "Etc/UTC"},
        {"database_path", "assistant.sqlite"}, {"context_max_chars", 50000}, {"check_interval_ms", 1000},
        {"llama", QJsonObject{{"completion_url", "http://127.0.0.1/v1/chat/completions"}, {"model", ""}, {"api_key", ""},
            {"timeout_ms", 1000}, {"max_response_tokens", 128}}},
        {"services", QJsonObject{{"timeout_ms", 1000}, {"user_agent", "MSA-test"},
            {"nominatim", "https://example.org/search"}, {"osrm_foot", "https://example.org/foot"},
            {"osrm_bike", "https://example.org/bike"}, {"osrm_driving", "https://example.org/driving"},
            {"weather", "https://example.org/forecast"}, {"search", "http://127.0.0.1/search"}}}};
}

TEST(ServerSettingsTest, ResolvesDatabaseRelativeToConfigAndRejectsMissingUnknownAndMistypedFields)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("server.json");
    const auto write = [path](const QJsonObject& object) {
        QFile file{path};
        if (!file.open(QIODevice::WriteOnly)) { throw std::runtime_error{"Test file"}; }
        const auto bytes = QJsonDocument{object}.toJson();
        EXPECT_EQ(file.write(bytes), bytes.size());
    };
    const auto valid = settingsObject();
    write(valid);
    const auto parsed = readServerSettings(path);
    EXPECT_EQ(parsed.databasePath, directory.filePath("assistant.sqlite"));
    EXPECT_EQ(parsed.port, 0);
    for (const auto& field : valid.keys()) {
        auto invalid = valid;
        invalid.remove(field);
        write(invalid);
        EXPECT_THROW(static_cast<void>(readServerSettings(path)), std::invalid_argument);
    }
    for (const auto& replacement : std::vector<QJsonObject>{{{"port", 1.5}}, {{"port", 65536}},
        {{"time_zone", "invalid-zone"}}, {{"context_max_chars", "50000"}}, {{"token", "<placeholder>"}}, {{"extra", true}}}) {
        auto invalid = valid;
        for (const auto& field : replacement.keys()) { invalid[field] = replacement.value(field); }
        write(invalid);
        EXPECT_THROW(static_cast<void>(readServerSettings(path)), std::invalid_argument);
    }
}

TEST(ServerNetworkTest, RealHttpWorkerKeepsReadRequestsAndReminderChecksAliveAndRejectsConcurrentWrites)
{
    QTemporaryDir directory;
    core::FakeClock clock{kNoon};
    QTcpServer http;
    ASSERT_TRUE(http.listen(QHostAddress::LocalHost, 0));
    auto config = settings(directory.filePath("assistant.sqlite"));
    config.llama.completionUrl = QUrl{QString{"http://127.0.0.1:%1/v1/chat/completions"}.arg(http.serverPort())};
    bool httpRequested{false};
    bool httpResponseSent{false};
    std::function<void()> duringHttp;
    QObject::connect(&http, &QTcpServer::newConnection, &http, [&http, &httpRequested, &httpResponseSent, &duringHttp]() {
        auto* socket = http.nextPendingConnection();
        auto bytes = std::make_shared<QByteArray>();
        auto handled = std::make_shared<bool>(false);
        QObject::connect(socket, &QTcpSocket::readyRead, socket,
            [socket, bytes, handled, &httpRequested, &httpResponseSent, &duringHttp]() {
                *bytes += socket->readAll();
                if (*handled || !bytes->contains("\r\n\r\n") || !bytes->contains("profile_facts")) { return; }
                *handled = true;
                httpRequested = true;
                if (duringHttp) { duringHttp(); }
                QTimer::singleShot(kHttpDelayMs, socket, [socket, &httpResponseSent]() {
                    httpResponseSent = true;
                    const QJsonObject content{{"reply", "Ответ после ожидания"}, {"tone", "Neutral"},
                        {"mood_recording", QJsonValue::Null}, {"profile_facts", QJsonArray{}}};
                    const QJsonObject envelope{{"choices", QJsonArray{QJsonObject{{"finish_reason", "stop"},
                        {"message", QJsonObject{{"role", "assistant"},
                        {"content", QString::fromUtf8(QJsonDocument{content}.toJson(QJsonDocument::Compact))}}}}}}};
                    const auto body = QJsonDocument{envelope}.toJson(QJsonDocument::Compact);
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    NetworkServices network{config};
    ServerRuntime server{config, clock, {network, network, network, network, network, network.geocodingSourceKey()}};
    ASSERT_TRUE(server.start());
    QTcpSocket client;
    QTcpSocket observer;
    connectClient(client, server.port());
    connectClient(observer, server.port());
    bool pingBeforeReply{false};
    bool reminderBeforeReply{false};
    QObject::connect(&observer, &QTcpSocket::readyRead, &observer,
        [&observer, &httpResponseSent, &pingBeforeReply, &reminderBeforeReply]() {
            if (httpResponseSent) { return; }
            const auto frames = observer.peek(observer.bytesAvailable()).split('\n');
            for (const auto& bytes : frames) {
                const auto frame = QJsonDocument::fromJson(bytes).object();
                if (frame.value("request_id") == "ping" && frame.value("ok").toBool()) { pingBeforeReply = true; }
                if (frame.value("name") == "reminder.due") { reminderBeforeReply = true; }
            }
        });
    send(client, infra::ProtocolRequest{"create-chat", "chat.create", {}});
    const auto chatId = receive(client, "create-chat").value("result").toObject().value("chat_id");
    send(client, infra::ProtocolRequest{"reminder", "reminder.create", {{"kind", "Timed"}, {"title", "Пора"}, {"due_at", "2026-11-09T12:00:01"}}});
    ASSERT_TRUE(receive(client, "reminder").value("ok").toBool());
    duringHttp = [&clock, &observer]() {
        clock.advance(std::chrono::seconds{2});
        send(observer, infra::ProtocolRequest{"ping", "ping", {}});
        send(observer, infra::ProtocolRequest{"busy", "task.create", {{"title", "Лишняя"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}}});
    };
    send(client, infra::ProtocolRequest{"send", "chat.send", {{"chat_id", chatId}, {"text", "Привет"}, {"search_query", QJsonValue::Null}}});
    std::vector<QJsonObject> events;
    const auto ping = receive(observer, "ping", &events);
    EXPECT_TRUE(ping.value("ok").toBool());
    const auto busy = receive(observer, "busy", &events);
    EXPECT_EQ(busy.value("error").toObject().value("code"), "busy");
    EXPECT_EQ(busy.value("error").toObject().value("effect"), "none");
    const auto reply = receive(client, "send", &events);
    EXPECT_TRUE(reply.value("ok").toBool()) << QJsonDocument(reply).toJson().toStdString();
    EXPECT_TRUE(httpRequested);
    EXPECT_TRUE(pingBeforeReply);
    EXPECT_TRUE(reminderBeforeReply);
    EXPECT_TRUE(std::ranges::any_of(events, [](const auto& event) { return event.value("name") == "reminder.due"; }));
}
} // namespace
} // namespace msa::server

