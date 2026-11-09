#include "FakeClock.h"
#include "msa/infra/ProtocolCodec.h"
#include "msa/infra/TcpServer.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace msa::infra
{
namespace
{
constexpr int kWaitMilliseconds{3000};
constexpr int kEventSliceMilliseconds{5};
constexpr unsigned long kPollMilliseconds{1};

bool waitUntil(const std::function<bool()>& predicate, int timeout = kWaitMilliseconds)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < timeout)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kEventSliceMilliseconds);
        QThread::msleep(kPollMilliseconds);
    }

    return predicate();
}

QByteArray wire(const ProtocolMessage& message)
{
    return std::get<QByteArray>(ProtocolCodec::encode(message));
}

void send(QTcpSocket& socket, const QByteArray& bytes)
{
    ASSERT_EQ(socket.write(bytes), bytes.size());
    socket.flush();
}

QJsonObject receive(QTcpSocket& socket)
{
    if (!waitUntil([&socket]() { return socket.canReadLine(); }))
    {
        ADD_FAILURE() << "Timed out waiting for a protocol frame";
        return {};
    }

    return QJsonDocument::fromJson(socket.readLine()).object();
}

class TcpServerTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void connectClient(QTcpSocket& socket);
    QJsonObject authenticate(QTcpSocket& socket, const QString& requestId = "auth-1");

    core::FakeClock m_clock{std::chrono::local_days{std::chrono::year{2026} / 11 / 8}};
    TcpServer m_server{"test-token", "Etc/UTC", m_clock};
    std::vector<std::pair<quint64, ProtocolRequest>> m_requests{};
};

void TcpServerTest::SetUp()
{
    ASSERT_TRUE(m_server.listen(QHostAddress::LocalHost, 0));
    QObject::connect(&m_server, &TcpServer::requestReceived, &m_server,
                     [this](quint64 connectionId, ProtocolRequest request)
                     { m_requests.emplace_back(connectionId, std::move(request)); });
}

void TcpServerTest::connectClient(QTcpSocket& socket)
{
    socket.connectToHost(QHostAddress::LocalHost, m_server.serverPort());
    ASSERT_TRUE(waitUntil([&socket]() { return socket.state() == QAbstractSocket::ConnectedState; }));
}

QJsonObject TcpServerTest::authenticate(QTcpSocket& socket, const QString& requestId)
{
    send(socket, wire(ProtocolAuth{requestId, kProtocolVersion, "test-token"}));

    return receive(socket);
}

TEST_F(TcpServerTest, AuthAdvertisesContractAndRequestCompletesOnlyOnce)
{
    QTcpSocket client;
    connectClient(client);
    const auto auth = authenticate(client);
    ASSERT_TRUE(auth.value("ok").toBool());
    const auto result = auth.value("result").toObject();
    EXPECT_EQ(result.value("version").toInt(), kProtocolVersion);
    EXPECT_EQ(result.value("max_frame_bytes").toInt(), kProtocolMaxFrameBytes);
    EXPECT_EQ(result.value("server_instance_id").toString(), m_server.serverInstanceId());
    EXPECT_EQ(result.value("server_time").toString(), "2026-11-08T00:00:00");
    EXPECT_EQ(result.value("time_zone").toString(), "Etc/UTC");
    send(client, wire(ProtocolRequest{"req-1", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 1; }));
    const auto connectionId = m_requests.front().first;
    EXPECT_FALSE(m_server.sendResponse(connectionId, ProtocolSuccess{"other", {}}));
    EXPECT_TRUE(m_server.sendResponse(connectionId, ProtocolSuccess{"req-1", {{"test", true}}}));
    EXPECT_FALSE(m_server.sendResponse(connectionId, ProtocolSuccess{"req-1", {}}));
    const auto response = receive(client);
    EXPECT_EQ(response.value("request_id").toString(), "req-1");
    EXPECT_TRUE(response.value("result").toObject().value("test").toBool());
}

TEST_F(TcpServerTest, WrongTokenAndVersionCloseWithoutDispatchOrSecret)
{
    for (const auto version : {kProtocolVersion, kProtocolVersion + 1})
    {
        QTcpSocket client;
        connectClient(client);
        send(client,
             wire(ProtocolAuth{"auth", version, "SECRET-wrong-token"}) + wire(ProtocolRequest{"req", "get_all", {}}));
        const auto response = receive(client);
        const auto error = response.value("error").toObject();
        EXPECT_EQ(error.value("code").toString(), version == kProtocolVersion ? "unauthorized" : "unsupported_version");
        EXPECT_FALSE(QJsonDocument(response).toJson().contains("SECRET"));
        EXPECT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    }
    EXPECT_TRUE(m_requests.empty());
}

TEST_F(TcpServerTest, FirstMessageMustBeAuthEvenForCorrelatedDecodeErrors)
{
    for (const auto& bytes :
         {wire(ProtocolRequest{"req", "get_all", {}}),
          QByteArray("{\"type\":\"request\",\"request_id\":\"req\",\"method\":\"unknown\",\"params\":{}}\n")})
    {
        QTcpSocket client;
        connectClient(client);
        send(client, bytes);
        EXPECT_EQ(receive(client).value("type").toString(), "protocol_error");
        EXPECT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    }
    EXPECT_TRUE(m_requests.empty());
}

TEST_F(TcpServerTest, AuthDeadlineClosesIncompleteAuth)
{
    QTcpSocket client;
    connectClient(client);
    send(client, "{\"type\":\"auth\"");
    constexpr int kAuthWaitMilliseconds{12000};
    ASSERT_TRUE(waitUntil([&client]() { return client.canReadLine(); }, kAuthWaitMilliseconds));
    EXPECT_EQ(receive(client).value("type").toString(), "protocol_error");
    EXPECT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    EXPECT_TRUE(m_requests.empty());
}

TEST_F(TcpServerTest, DuplicateIdsIncludingAuthAndRejectedRequestsNeverDispatchAgain)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    send(client,
         wire(ProtocolAuth{"auth-2", kProtocolVersion, "test-token"}) + wire(ProtocolRequest{"auth-1", "get_all", {}}) +
             QByteArray("{\"type\":\"request\",\"request_id\":\"bad\",\"method\":\"unknown\",\"params\":{}}\n") +
             wire(ProtocolRequest{"bad", "get_all", {}}) + wire(ProtocolRequest{"req", "get_all", {}}) +
             wire(ProtocolRequest{"req", "get_all", {}}));
    for (const auto& code :
         {"invalid_request", "invalid_request", "unknown_method", "invalid_request", "invalid_request"})
    {
        EXPECT_EQ(receive(client).value("error").toObject().value("code").toString(), code);
    }
    ASSERT_EQ(m_requests.size(), 1);
    EXPECT_TRUE(m_server.sendResponse(m_requests.front().first, ProtocolSuccess{"req", {}}));
    EXPECT_TRUE(receive(client).value("ok").toBool());
    send(client, wire(ProtocolRequest{"req", "get_all", {}}));
    EXPECT_EQ(receive(client).value("error").toObject().value("code").toString(), "invalid_request");
    EXPECT_EQ(m_requests.size(), 1);
}

TEST_F(TcpServerTest, BadParametersKeepSessionAndDoNotReachHandler)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    send(client, "{\"type\":\"request\",\"request_id\":\"bad\",\"method\":\"get_all\",\"params\":{\"extra\":1}}\n");
    EXPECT_EQ(receive(client).value("error").toObject().value("code").toString(), "invalid_request");
    send(client, wire(ProtocolRequest{"good", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 1; }));
    EXPECT_EQ(m_requests.front().second.requestId, "good");
}

TEST_F(TcpServerTest, FragmentedUtf8AndCoalescedFramesPreserveTextAndOrder)
{
    QTcpSocket client;
    connectClient(client);
    const auto auth = wire(ProtocolAuth{"auth", kProtocolVersion, "test-token"});
    for (const auto byte : auth)
    {
        send(client, QByteArray(1, byte));
        QCoreApplication::processEvents();
    }
    ASSERT_TRUE(receive(client).value("ok").toBool());
    const auto request = wire(
        ProtocolRequest{"first",
                        "command.execute",
                        {{"text", QStringLiteral("Привет 🌙")}, {"chat_id", "1"}, {"reference", QJsonValue::Null}}});
    const auto split = request.indexOf(QStringLiteral("🌙").toUtf8()) + 1;
    send(client, request.first(split));
    QCoreApplication::processEvents();
    EXPECT_TRUE(m_requests.empty());
    send(client, request.sliced(split) + wire(ProtocolRequest{"second", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 2; }));
    EXPECT_EQ(m_requests.front().second.params.value("text").toString(), QStringLiteral("Привет 🌙"));
    EXPECT_EQ(m_requests.back().second.requestId, "second");
}

TEST_F(TcpServerTest, FatalFrameStopsFollowingCommandsAndOtherClientStillWorks)
{
    for (const auto& bad : {QByteArray("\n"), QByteArray("invalid\n"), QByteArray("[]\n"), QByteArray("\xff\n", 2),
                            QByteArray(kProtocolMaxFrameBytes + 1, 'x')})
    {
        QTcpSocket client;
        connectClient(client);
        ASSERT_TRUE(authenticate(client).value("ok").toBool());
        send(client, bad + wire(ProtocolRequest{"after", "get_all", {}}));
        EXPECT_EQ(receive(client).value("type").toString(), "protocol_error");
        EXPECT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    }
    EXPECT_TRUE(m_requests.empty());
    QTcpSocket healthy;
    connectClient(healthy);
    EXPECT_TRUE(authenticate(healthy).value("ok").toBool());
}

TEST_F(TcpServerTest, ClientCannotSendServerResponsesOrEvents)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    send(client, wire(ProtocolSuccess{"forged", {}}));
    EXPECT_EQ(receive(client).value("type").toString(), "protocol_error");
    EXPECT_TRUE(waitUntil([&client]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    EXPECT_TRUE(m_requests.empty());
}

TEST_F(TcpServerTest, PendingRepliesStayWithOriginalConnectionAndMayFinishOutOfOrder)
{
    QTcpSocket first;
    QTcpSocket second;
    connectClient(first);
    connectClient(second);
    ASSERT_TRUE(authenticate(first).value("ok").toBool());
    ASSERT_TRUE(authenticate(second).value("ok").toBool());
    send(first, wire(ProtocolRequest{"same", "get_all", {}}) + wire(ProtocolRequest{"next", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 2; }));
    send(second, wire(ProtocolRequest{"same", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 3; }));
    const auto firstId = m_requests.front().first;
    const auto secondId = m_requests.back().first;
    ASSERT_NE(firstId, secondId);
    EXPECT_TRUE(m_server.sendResponse(firstId, ProtocolSuccess{"next", {}}));
    EXPECT_EQ(receive(first).value("request_id").toString(), "next");
    EXPECT_TRUE(m_server.sendResponse(secondId, ProtocolFailure{"same", "llm_unavailable", "Ошибка модели.",
                                                                "user_saved", QString{"9223372036854775807"}}));
    const auto error = receive(second).value("error").toObject();
    EXPECT_EQ(error.value("effect").toString(), "user_saved");
    EXPECT_EQ(error.value("user_message_id").toString(), "9223372036854775807");
    EXPECT_TRUE(m_server.sendResponse(firstId, ProtocolSuccess{"same", {}}));
    EXPECT_EQ(receive(first).value("request_id").toString(), "same");
}

TEST_F(TcpServerTest, DisconnectDropsTailAndLateCompletionCannotReachReconnectedClient)
{
    QTcpSocket first;
    connectClient(first);
    ASSERT_TRUE(authenticate(first).value("ok").toBool());
    send(first, wire(ProtocolRequest{"same", "get_all", {}}) + QByteArray("{\"type\":\"request\""));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 1; }));
    const auto oldId = m_requests.front().first;
    bool closed{false};
    QObject::connect(&m_server, &TcpServer::connectionClosed, &m_server,
                     [&closed, oldId](quint64 connectionId) { closed = connectionId == oldId; });
    first.abort();
    ASSERT_TRUE(waitUntil([&closed]() { return closed; }));
    QTcpSocket second;
    connectClient(second);
    ASSERT_TRUE(authenticate(second).value("ok").toBool());
    send(second, wire(ProtocolRequest{"same", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 2; }));
    EXPECT_FALSE(m_server.sendResponse(oldId, ProtocolSuccess{"same", {{"late", true}}}));
    EXPECT_TRUE(m_server.sendResponse(m_requests.back().first, ProtocolSuccess{"same", {{"fresh", true}}}));
    EXPECT_TRUE(receive(second).value("result").toObject().value("fresh").toBool());
}

TEST_F(TcpServerTest, EventsGoOnlyToAuthenticatedClientsWithSharedUniqueEventId)
{
    QTcpSocket first;
    QTcpSocket second;
    QTcpSocket unauthenticated;
    connectClient(first);
    connectClient(second);
    connectClient(unauthenticated);
    ASSERT_TRUE(authenticate(first).value("ok").toBool());
    ASSERT_TRUE(authenticate(second).value("ok").toBool());
    EXPECT_FALSE(m_server.publishEvent("unknown", {}));
    ASSERT_TRUE(m_server.publishEvent("state.changed", {{"scopes", QJsonArray{"tasks"}}}));
    const auto firstEvent = receive(first);
    const auto secondEvent = receive(second);
    EXPECT_EQ(firstEvent, secondEvent);
    EXPECT_EQ(firstEvent.value("server_instance_id").toString(), m_server.serverInstanceId());
    ASSERT_TRUE(m_server.publishEvent("chat.changed", {{"chat_id", "1"}}));
    EXPECT_NE(receive(first).value("event_id"), firstEvent.value("event_id"));
    static_cast<void>(receive(second));
    // Ответ auth должен оказаться первым кадром этого соединения.
    EXPECT_EQ(authenticate(unauthenticated).value("type").toString(), "response");
}

TEST_F(TcpServerTest, OversizedResponseIsCorrelatedAndDoesNotTruncateOrClaimNoChanges)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    send(client, wire(ProtocolRequest{"large", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 1; }));
    const QString text(kProtocolMaxFrameBytes, QChar{'x'});
    ASSERT_TRUE(m_server.sendResponse(m_requests.front().first, ProtocolSuccess{"large", {{"text", text}}}));
    const auto response = receive(client);
    EXPECT_EQ(response.value("request_id").toString(), "large");
    EXPECT_EQ(response.value("error").toObject().value("code").toString(), "response_too_large");
    EXPECT_EQ(response.value("error").toObject().value("effect").toString(), "unknown");
    EXPECT_EQ(text.size(), kProtocolMaxFrameBytes);
    EXPECT_FALSE(m_server.sendResponse(m_requests.front().first, ProtocolSuccess{"large", {}}));
}

TEST_F(TcpServerTest, QueuedHandlerDoesNotBlockAnotherRequestOrTimers)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    QObject::connect(
        &m_server, &TcpServer::requestReceived, &m_server,
        [this](quint64 connectionId, ProtocolRequest request)
        {
            if (request.requestId == "fast")
            {
                EXPECT_TRUE(m_server.sendResponse(connectionId, ProtocolSuccess{request.requestId, {}}));
            }
        },
        Qt::QueuedConnection);
    bool timerFired{false};
    QTimer::singleShot(0, &m_server, [&timerFired]() { timerFired = true; });
    send(client, wire(ProtocolRequest{"slow", "get_all", {}}) + wire(ProtocolRequest{"fast", "get_all", {}}));
    EXPECT_EQ(receive(client).value("request_id").toString(), "fast");
    EXPECT_TRUE(timerFired);
    ASSERT_EQ(m_requests.size(), 2);
    EXPECT_TRUE(m_server.sendResponse(m_requests.front().first, ProtocolSuccess{"slow", {}}));
    EXPECT_EQ(receive(client).value("request_id").toString(), "slow");
}

TEST_F(TcpServerTest, HandlerExceptionIsSanitizedAndConnectionRemainsUsable)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    const auto handler =
        QObject::connect(&m_server, &TcpServer::requestReceived, &m_server,
                         [](quint64, const ProtocolRequest&) { throw std::runtime_error("SECRET sql and token"); });
    send(client, wire(ProtocolRequest{"throws", "get_all", {}}));
    const auto response = receive(client);
    EXPECT_EQ(response.value("error").toObject().value("effect").toString(), "unknown");
    EXPECT_FALSE(QJsonDocument(response).toJson().contains("SECRET"));
    QObject::disconnect(handler);
    send(client, wire(ProtocolRequest{"next", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 2; }));
    EXPECT_TRUE(m_server.sendResponse(m_requests.back().first, ProtocolSuccess{"next", {}}));
    EXPECT_TRUE(receive(client).value("ok").toBool());
}

TEST_F(TcpServerTest, CloseAllowsRestartWithoutReusingConnectionId)
{
    QTcpSocket client;
    connectClient(client);
    ASSERT_TRUE(authenticate(client).value("ok").toBool());
    send(client, wire(ProtocolRequest{"req", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 1; }));
    const auto instanceId = m_server.serverInstanceId();
    m_server.close();
    ASSERT_TRUE(m_server.listen(QHostAddress::LocalHost, 0));
    QTcpSocket second;
    connectClient(second);
    ASSERT_TRUE(authenticate(second).value("ok").toBool());
    send(second, wire(ProtocolRequest{"req", "get_all", {}}));
    ASSERT_TRUE(waitUntil([this]() { return m_requests.size() == 2; }));
    EXPECT_NE(m_requests.front().first, m_requests.back().first);
    EXPECT_EQ(instanceId, m_server.serverInstanceId());
}
} // namespace
} // namespace msa::infra
