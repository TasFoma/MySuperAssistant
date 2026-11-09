#include "ProtocolTestClient.h"

#include "msa/infra/TcpClient.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace msa::infra::test {
namespace {
using namespace std::chrono_literals;
constexpr int kQuietPeriodMs{100};
constexpr int kInvalidMaximumFrame{2097152};

QJsonObject authResult(const QString& instance = "server-instance")
{
    return {{"version", 1}, {"server_instance_id", instance}, {"max_frame_bytes", kProtocolMaxFrameBytes},
        {"server_time", "2026-11-09T12:00:00"}, {"time_zone", "Etc/UTC"}};
}

QJsonObject snapshot()
{
    return {{"server_time", "2026-11-09T12:00:00"}, {"time_zone", "Etc/UTC"}, {"main_chat_id", "1"},
        {"tasks", QJsonArray{}}, {"reminders", QJsonArray{}}, {"habits", QJsonArray{}}, {"events_today", QJsonArray{}}};
}

QJsonObject success(const QString& requestId, const QJsonObject& result)
{
    return {{"type", "response"}, {"request_id", requestId}, {"ok", true}, {"result", result}};
}

QJsonObject failure(const QString& requestId, const QString& code, const QString& effect = "none",
    const QJsonValue& userId = QJsonValue::Null)
{
    return {{"type", "response"}, {"request_id", requestId}, {"ok", false}, {"error", QJsonObject{
        {"code", code}, {"message", "Понятная причина"}, {"effect", effect}, {"user_message_id", userId}}}};
}

QJsonObject event(const QString& instance = "server-instance", const QString& eventId = "event-1")
{
    return {{"type", "event"}, {"server_instance_id", instance}, {"event_id", eventId},
        {"name", "state.changed"}, {"data", QJsonObject{{"scopes", QJsonArray{"tasks"}}}}};
}

QByteArray wire(const QJsonObject& object)
{
    return QJsonDocument{object}.toJson(QJsonDocument::Compact) + '\n';
}

TcpClientSettings settings(quint16 port)
{
    return {"127.0.0.1", port, "client-test-token", 300ms, 500ms, 5ms, 15ms};
}

void quietPeriod()
{
    bool done{false};
    QObject context;
    QTimer::singleShot(kQuietPeriodMs, &context, [&done]() { done = true; });
    ASSERT_TRUE(waitForTest([&done]() { return done; }));
}

class TestPeer {
public:
    TestPeer();
    [[nodiscard]] bool listen(quint16 port = 0);
    [[nodiscard]] quint16 port() const;
    void send(const QJsonObject& object);
    void sendBytes(const QByteArray& bytes);
    void drop(bool stopListening = false);
    [[nodiscard]] int methodCount(const QString& method) const;
    [[nodiscard]] int connections() const;

    bool autoAuth{true};
    bool autoSnapshot{true};
    QJsonObject auth{authResult()};
    QJsonObject initial{snapshot()};
    std::vector<QJsonObject> received{};
    std::function<void(const QJsonObject&)> handler{};

private:
    QTcpServer m_listener{};
    QPointer<QTcpSocket> m_socket{};
    int m_connections{0};
};

TestPeer::TestPeer()
{
    QObject::connect(&m_listener, &QTcpServer::newConnection, &m_listener, [this]() {
        while (m_listener.hasPendingConnections()) {
            auto* socket = m_listener.nextPendingConnection();
            m_socket = socket;
            ++m_connections;
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                while (socket->canReadLine()) {
                    const auto frame = QJsonDocument::fromJson(socket->readLine()).object();
                    received.push_back(frame);
                    const auto requestId = frame.value("request_id").toString();
                    if (frame.value("type") == "auth" && autoAuth) { send(success(requestId, auth)); }
                    else if (frame.value("method") == "get_all" && autoSnapshot) { send(success(requestId, initial)); }
                    if (handler) { handler(frame); }
                }
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

bool TestPeer::listen(quint16 port)
{
    return m_listener.listen(QHostAddress::LocalHost, port);
}

quint16 TestPeer::port() const { return m_listener.serverPort(); }
int TestPeer::connections() const { return m_connections; }

void TestPeer::send(const QJsonObject& object) { sendBytes(wire(object)); }

void TestPeer::sendBytes(const QByteArray& bytes)
{
    if (!m_socket || m_socket->write(bytes) != bytes.size()) { throw std::runtime_error{"Test peer is disconnected"}; }
    m_socket->flush();
}

void TestPeer::drop(bool stopListening)
{
    if (stopListening) { m_listener.close(); }
    if (m_socket) { m_socket->abort(); }
}

int TestPeer::methodCount(const QString& method) const
{
    return static_cast<int>(std::ranges::count_if(received, [method](const auto& frame) { return frame.value("method") == method; }));
}

class TcpClientTest : public ::testing::Test {
protected:
    void SetUp() override;
    void connectReady();

    TestPeer m_peer{};
    std::unique_ptr<TcpClient> m_client{};
    std::vector<ProtocolSuccess> m_responses{};
    std::vector<ProtocolFailure> m_rejections{};
    std::vector<TcpClientRequestFailure> m_failures{};
    std::vector<TcpClientState> m_states{};
    std::vector<ProtocolEvent> m_events{};
};

void TcpClientTest::SetUp()
{
    ASSERT_TRUE(m_peer.listen());
    m_client = std::make_unique<TcpClient>(settings(m_peer.port()));
    QObject::connect(m_client.get(), &TcpClient::responseReceived, m_client.get(),
        [this](ProtocolSuccess value) { m_responses.push_back(std::move(value)); });
    QObject::connect(m_client.get(), &TcpClient::serverRejected, m_client.get(),
        [this](ProtocolFailure value) { m_rejections.push_back(std::move(value)); });
    QObject::connect(m_client.get(), &TcpClient::requestFailed, m_client.get(),
        [this](TcpClientRequestFailure value) { m_failures.push_back(std::move(value)); });
    QObject::connect(m_client.get(), &TcpClient::stateChanged, m_client.get(),
        [this](TcpClientState value) { m_states.push_back(value); });
    QObject::connect(m_client.get(), &TcpClient::eventReceived, m_client.get(),
        [this](ProtocolEvent value) { m_events.push_back(std::move(value)); });
}

void TcpClientTest::connectReady()
{
    m_client->connectToServer();
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Ready; }));
}

TEST_F(TcpClientTest, AuthenticatesDownloadsSnapshotAndBecomesReadyBeforeApplicationRequests)
{
    EXPECT_EQ(m_client->state(), TcpClientState::Disconnected);
    const auto early = m_client->sendRequest("task.create", {{"title", "Задача"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}});
    EXPECT_TRUE(m_failures.empty());
    connectReady();
    ASSERT_EQ(m_failures.size(), 1U);
    EXPECT_EQ(m_failures.front().requestId, early);
    EXPECT_EQ(m_failures.front().effect, "none");
    EXPECT_EQ(m_peer.methodCount("task.create"), 0);
    EXPECT_EQ(m_peer.methodCount("get_all"), 1);
    EXPECT_EQ(m_peer.received.front().value("type"), "auth");
    EXPECT_EQ(m_client->initialSnapshot(), snapshot());
    ASSERT_TRUE(m_client->serverInfo());
    EXPECT_EQ(m_client->serverInfo()->instanceId, "server-instance");
    EXPECT_EQ(m_client->connectionAttempts(), 0);
    EXPECT_EQ(m_states, (std::vector{TcpClientState::Connecting, TcpClientState::Authenticating,
        TcpClientState::Synchronizing, TcpClientState::Ready}));
}

TEST_F(TcpClientTest, CorrelatesReorderedResponsesAndIgnoresUnknownOrRepeatedIds)
{
    connectReady();
    const auto first = m_client->sendRequest("ping");
    const auto second = m_client->sendRequest("ping");
    ASSERT_TRUE(waitForTest([this]() { return m_peer.methodCount("ping") == 2; }));
    m_peer.sendBytes(wire(success("unknown", {{"ignored", true}})) + wire(success(second, {{"order", 2}}))
        + wire(success(second, {{"duplicate", true}})) + wire(success(first, {{"order", 1}})));
    ASSERT_TRUE(waitForTest([this]() { return m_responses.size() == 2; }));
    EXPECT_EQ(m_responses[0].requestId, second);
    EXPECT_EQ(m_responses[1].requestId, first);
    EXPECT_EQ(m_client->state(), TcpClientState::Ready);
}

TEST_F(TcpClientTest, PreservesServerUserSavedFailureAndSupportsQueuedTypedSignals)
{
    connectReady();
    bool queued{false};
    QObject receiver;
    QObject::connect(m_client.get(), &TcpClient::serverRejected, &receiver,
        [&queued](ProtocolFailure value) { queued = value.effect == "user_saved" && value.userMessageId == "17"; },
        Qt::QueuedConnection);
    const auto requestId = m_client->sendRequest("chat.send", {{"chat_id", "1"}, {"text", "Вопрос"}, {"search_query", QJsonValue::Null}});
    ASSERT_TRUE(waitForTest([this]() { return m_peer.methodCount("chat.send") == 1; }));
    m_peer.send(failure(requestId, "llm_unavailable", "user_saved", "17"));
    ASSERT_TRUE(waitForTest([&queued]() { return queued; }));
    ASSERT_EQ(m_rejections.size(), 1U);
    EXPECT_EQ(m_rejections.front().requestId, requestId);
    EXPECT_TRUE(m_failures.empty());
}

TEST_F(TcpClientTest, DeduplicatesEventsAndRejectsAnotherServerInstance)
{
    connectReady();
    m_peer.sendBytes(wire(event()) + wire(event()) + wire(event("server-instance", "event-2")));
    ASSERT_TRUE(waitForTest([this]() { return m_events.size() == 2; }));
    m_peer.send(event("another-server", "event-3"));
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Disconnected; }));
    EXPECT_EQ(m_events.size(), 2U);
    quietPeriod();
    EXPECT_EQ(m_peer.connections(), 1);
}

TEST_F(TcpClientTest, ReassemblesSplitUtf8AndDoesNotCarryEofTailIntoReconnection)
{
    connectReady();
    const auto requestId = m_client->sendRequest("ping");
    const auto bytes = wire(success(requestId, {{"text", "Полный ответ 🙂"}}));
    const auto split = bytes.indexOf(QByteArray::fromHex("f09f9982")) + 1;
    ASSERT_GT(split, 0);
    m_peer.sendBytes(bytes.first(split));
    quietPeriod();
    EXPECT_TRUE(m_responses.empty());
    m_peer.sendBytes(bytes.sliced(split));
    ASSERT_TRUE(waitForTest([this]() { return m_responses.size() == 1; }));
    EXPECT_EQ(m_responses.front().result.value("text"), "Полный ответ 🙂");
    const auto lost = m_client->sendRequest("ping");
    ASSERT_TRUE(waitForTest([this]() { return m_peer.methodCount("ping") == 2; }));
    m_peer.sendBytes(wire(success(lost, {{"unfinished", true}})).first(20));
    quietPeriod();
    m_peer.drop();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 2 && m_client->state() == TcpClientState::Ready; }));
    ASSERT_TRUE(waitForTest([this]() { return m_failures.size() == 1; }));
    EXPECT_EQ(m_failures.front().requestId, lost);
    EXPECT_EQ(m_failures.front().effect, "none");
    EXPECT_EQ(m_responses.size(), 1U);
    EXPECT_EQ(m_peer.methodCount("get_all"), 2);
}

TEST_F(TcpClientTest, LostWritesBecomeUnknownAndOnlyHandshakeIsRepeated)
{
    connectReady();
    const auto writeId = m_client->sendRequest("task.create", {{"title", "Единственная"}, {"due_date", QJsonValue::Null}, {"priority", "Normal"}});
    const auto readId = m_client->sendRequest("ping");
    ASSERT_TRUE(waitForTest([this]() { return m_peer.methodCount("ping") == 1; }));
    m_peer.drop();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 2 && m_client->state() == TcpClientState::Ready && m_failures.size() == 2; }));
    const auto write = std::ranges::find(m_failures, writeId, &TcpClientRequestFailure::requestId);
    const auto read = std::ranges::find(m_failures, readId, &TcpClientRequestFailure::requestId);
    ASSERT_NE(write, m_failures.end());
    ASSERT_NE(read, m_failures.end());
    EXPECT_EQ(write->effect, "unknown");
    EXPECT_EQ(read->effect, "none");
    EXPECT_EQ(m_peer.methodCount("task.create"), 1);
    EXPECT_EQ(m_peer.methodCount("ping"), 1);
    EXPECT_EQ(m_peer.methodCount("get_all"), 2);
    const auto fresh = m_client->sendRequest("ping");
    EXPECT_NE(fresh, readId);
    m_peer.send(success(writeId, {{"task_id", "5"}}));
    m_peer.send(success(fresh, {{"server_time", "2026-11-09T12:00:00"}}));
    ASSERT_TRUE(waitForTest([this]() { return m_responses.size() == 1; }));
    EXPECT_EQ(m_responses.front().requestId, fresh);
}

TEST_F(TcpClientTest, InvalidOrOversizedRequestsFailLocallyAndNeverReachSocket)
{
    connectReady();
    static_cast<void>(m_client->sendRequest("unknown.method"));
    static_cast<void>(m_client->sendRequest("task.delete", {{"task_id", 7}}));
    static_cast<void>(m_client->sendRequest("chat.send", {{"chat_id", "1"},
        {"text", QString(kProtocolMaxFrameBytes, 'x')}, {"search_query", QJsonValue::Null}}));
    ASSERT_TRUE(waitForTest([this]() { return m_failures.size() == 3; }));
    EXPECT_EQ(m_peer.received.size(), 2U);
    EXPECT_TRUE(std::ranges::all_of(m_failures, [](const auto& value) { return value.effect == "none"; }));
    EXPECT_EQ(m_client->state(), TcpClientState::Ready);
}

TEST_F(TcpClientTest, SuccessfulSessionResetsBudgetAndNextOutageGetsFiveAttempts)
{
    std::vector<int> attempts;
    QObject::connect(m_client.get(), &TcpClient::connectionAttempt, m_client.get(),
        [&attempts](int value) { attempts.push_back(value); });
    connectReady();
    const auto port = m_peer.port();
    m_peer.drop(true);
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Exhausted; }));
    EXPECT_EQ(attempts, (std::vector{1, 1, 2, 3, 4, 5}));
    ASSERT_TRUE(m_peer.listen(port));
    quietPeriod();
    EXPECT_EQ(m_peer.connections(), 1);
    m_client->reconnect();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 2 && m_client->state() == TcpClientState::Ready; }));
}

TEST_F(TcpClientTest, InvalidAuthOrSnapshotStopsWithoutAutomaticRetry)
{
    m_peer.auth["max_frame_bytes"] = kInvalidMaximumFrame;
    m_client->connectToServer();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 1 && m_client->state() == TcpClientState::Disconnected; }));
    quietPeriod();
    EXPECT_EQ(m_peer.connections(), 1);
    EXPECT_EQ(m_peer.methodCount("get_all"), 0);
    m_peer.auth = authResult();
    m_peer.initial["main_chat_id"] = 1;
    m_client->reconnect();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 2 && m_client->state() == TcpClientState::Disconnected; }));
    EXPECT_FALSE(m_client->serverInfo());
    EXPECT_TRUE(m_client->initialSnapshot().isEmpty());
}

TEST_F(TcpClientTest, BadTokenIsTerminalAndDoesNotSpendFiveAttempts)
{
    m_peer.autoAuth = false;
    m_peer.handler = [this](const auto& frame) {
        if (frame.value("type") == "auth") {
            m_peer.send(failure(frame.value("request_id").toString(), "unauthorized"));
            m_peer.drop();
        }
    };
    m_client->connectToServer();
    ASSERT_TRUE(waitForTest([this]() { return m_peer.connections() == 1 && m_client->state() == TcpClientState::Disconnected; }));
    quietPeriod();
    EXPECT_EQ(m_peer.connections(), 1);
}

TEST_F(TcpClientTest, TimeoutAfterSendingWriteClosesSessionAndDoesNotReplayIt)
{
    connectReady();
    const auto requestId = m_client->sendRequest("task.delete", {{"task_id", "17"}});
    ASSERT_TRUE(waitForTest([this]() { return m_failures.size() == 1 && m_peer.connections() == 2 && m_client->state() == TcpClientState::Ready; }));
    EXPECT_EQ(m_failures.front().requestId, requestId);
    EXPECT_EQ(m_failures.front().code, "request_timeout");
    EXPECT_EQ(m_failures.front().effect, "unknown");
    EXPECT_EQ(m_peer.methodCount("task.delete"), 1);
}

TEST_F(TcpClientTest, AuthAndSnapshotTimeoutsExhaustExactlyFiveAttempts)
{
    m_peer.autoAuth = false;
    m_client->connectToServer();
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Exhausted; }));
    EXPECT_EQ(m_peer.connections(), 5);
    EXPECT_EQ(m_client->connectionAttempts(), 5);
    m_peer.autoAuth = true;
    m_peer.autoSnapshot = false;
    m_client->reconnect();
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Exhausted; }));
    EXPECT_EQ(m_peer.connections(), 10);
    EXPECT_EQ(m_peer.methodCount("get_all"), 5);
    quietPeriod();
    EXPECT_EQ(m_peer.connections(), 10);
}

TEST_F(TcpClientTest, MalformedFrameFailsPendingWriteWithoutExecutingFollowingResponse)
{
    connectReady();
    const auto requestId = m_client->sendRequest("task.delete", {{"task_id", "17"}});
    ASSERT_TRUE(waitForTest([this]() { return m_peer.methodCount("task.delete") == 1; }));
    m_peer.sendBytes("[]\n" + wire(success(requestId, {{"deleted", true}})));
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Disconnected && m_failures.size() == 1; }));
    EXPECT_EQ(m_failures.front().effect, "unknown");
    EXPECT_TRUE(m_responses.empty());
}

TEST(TcpClientRetryTest, FiveFailedAttemptsStopUntilManualActionAndBackoffIsCapped)
{
    QTcpServer reservation;
    ASSERT_TRUE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto port = reservation.serverPort();
    reservation.close();
    TcpClient client{settings(port)};
    std::vector<int> attempts;
    std::vector<int> delays;
    QObject::connect(&client, &TcpClient::connectionAttempt, &client, [&attempts](int value) { attempts.push_back(value); });
    QObject::connect(&client, &TcpClient::retryScheduled, &client,
        [&delays](int, int delay) { delays.push_back(delay); });
    client.connectToServer();
    client.connectToServer();
    ASSERT_TRUE(waitForTest([&client]() { return client.state() == TcpClientState::Exhausted; }));
    EXPECT_EQ(attempts, (std::vector{1, 2, 3, 4, 5}));
    EXPECT_EQ(delays, (std::vector{5, 10, 15, 15}));
    TestPeer peer;
    ASSERT_TRUE(peer.listen(port));
    quietPeriod();
    EXPECT_EQ(peer.connections(), 0);
    EXPECT_EQ(client.state(), TcpClientState::Exhausted);
    client.reconnect();
    ASSERT_TRUE(waitForTest([&client]() { return client.state() == TcpClientState::Ready; }));
    EXPECT_EQ(peer.connections(), 1);
    EXPECT_EQ(client.connectionAttempts(), 0);
}

TEST(TcpClientRetryTest, ExplicitDisconnectCancelsRemainingAttempts)
{
    QTcpServer reservation;
    ASSERT_TRUE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto port = reservation.serverPort();
    reservation.close();
    TcpClient client{settings(port)};
    int attempts{0};
    QObject::connect(&client, &TcpClient::connectionAttempt, &client, [&attempts](int) { ++attempts; });
    QObject::connect(&client, &TcpClient::retryScheduled, &client, [&client](int, int) { client.disconnectFromServer(); });
    client.connectToServer();
    ASSERT_TRUE(waitForTest([&client, &attempts]() { return attempts == 1 && client.state() == TcpClientState::Disconnected; }));
    quietPeriod();
    EXPECT_EQ(attempts, 1);
}

TEST(TcpClientRetryTest, RejectsInvalidSettingsWithoutLeakingPersonalValues)
{
    auto config = settings(1);
    config.token = " ";
    EXPECT_THROW(static_cast<void>(TcpClient{config}), std::invalid_argument);
    config = settings(1);
    config.initialReconnectDelay = 20ms;
    EXPECT_THROW(static_cast<void>(TcpClient{config}), std::invalid_argument);
}
} // namespace
} // namespace msa::infra::test

