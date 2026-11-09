#include "FakeClock.h"
#include "msa/desktop/DesktopRuntime.h"
#include "msa/infra/JsonClientPreferences.h"
#include "msa/infra/JsonMusicPreferences.h"
#include "msa/infra/ProtocolPayload.h"
#include "msa/infra/TcpServer.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStringList>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QThread>
#include <QVariant>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace
{
using namespace std::chrono_literals;
constexpr int kWaitMilliseconds = 5000;
constexpr int kQuietMilliseconds = 100;
constexpr int kStopMilliseconds = 3000;
constexpr std::uint32_t kMusicSeed = 42;
constexpr auto kMoment = std::chrono::local_days{std::chrono::year{2026} / 11 / 9} + 12h;
const QString kToken{"desktop-test-token"};
bool waitFor(const std::function<bool()>& predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kWaitMilliseconds)
    {
        QGuiApplication::processEvents();
        if (predicate())
        {
            return true;
        }
        QThread::msleep(1);
    }
    return false;
}
void settle()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kQuietMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
}
msa::desktop::DesktopRuntimeOptions options(const QTemporaryDir& directory)
{
    return {directory.filePath("client.json"), directory.filePath("music.json"), kMusicSeed, 300ms, 2000ms, 2ms, 4ms};
}
msa::core::ClientPreferences preferences(quint16 port, bool autoConnect = true)
{
    return {"127.0.0.1", port, kToken.toStdString(), autoConnect};
}
void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(bytes), bytes.size());
}
class LocalServer final
{
public:
    explicit LocalServer(QString title);
    msa::core::FakeClock m_clock{kMoment};
    msa::infra::TcpServer m_server{kToken, "Etc/UTC", m_clock};
    QString m_taskTitle;
    int m_reads{};
    int m_writes{};
    bool m_holdWrite{false};
    std::optional<std::pair<quint64, QString>> m_held{};
};
LocalServer::LocalServer(QString title) : m_taskTitle{std::move(title)}
{
    QObject::connect(
        &m_server, &msa::infra::TcpServer::requestReceived, &m_server,
        [this](quint64 connection, const msa::infra::ProtocolRequest& request)
        {
            QJsonObject result;
            if (request.method == "get_all")
            {
                ++m_reads;
                const msa::core::Task task{
                    1, m_taskTitle.toStdString(), {}, msa::core::TaskPriority::Normal, false, kMoment};
                result = {{"server_time", "2026-11-09T12:00:00"},
                          {"time_zone", "Etc/UTC"},
                          {"main_chat_id", "1"},
                          {"tasks", QJsonArray{msa::infra::ProtocolPayload::object(task)}},
                          {"reminders", QJsonArray{}},
                          {"habits", QJsonArray{}},
                          {"events_today", QJsonArray{}}};
            }
            else if (request.method == "chat.list")
            {
                result = {{"chats", QJsonArray{QJsonObject{{"id", "1"}, {"title", "Основной"}}}},
                          {"main_chat_id", "1"}};
            }
            else if (request.method == "chat.messages")
            {
                result = {{"chat_id", "1"},
                          {"messages", QJsonArray{}},
                          {"next_after_message_id", QJsonValue::Null},
                          {"has_more", false}};
            }
            else if (request.method == "profile.get")
            {
                result = {{"facts", QJsonArray{}}};
            }
            else if (request.method == "mood.history")
            {
                result = {{"entries", QJsonArray{}}};
            }
            else if (request.method == "route.history")
            {
                result = {{"recent", QJsonArray{}},
                          {"statistics", msa::infra::ProtocolPayload::object(msa::core::RouteCalculationStatistics{})}};
            }
            else if (request.method == "task.set_done")
            {
                ++m_writes;
                if (m_holdWrite)
                {
                    m_held = std::pair{connection, request.requestId};
                    return;
                }
                result = {{"task_id", "1"}};
            }
            else
            {
                ADD_FAILURE() << request.method.toStdString();
            }
            EXPECT_TRUE(m_server.sendResponse(connection, msa::infra::ProtocolSuccess{request.requestId, result}));
        });
}
class DesktopRuntimeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_directory.isValid());
    }
    void TearDown() override
    {
        EXPECT_TRUE(m_warnings.isEmpty()) << m_warnings.join('\n').toStdString();
    }
    std::unique_ptr<QQmlApplicationEngine> openWindow(msa::desktop::DesktopRuntime& runtime);
    QTemporaryDir m_directory{};
    QStringList m_warnings{};
};
std::unique_ptr<QQmlApplicationEngine> DesktopRuntimeTest::openWindow(msa::desktop::DesktopRuntime& runtime)
{
    auto engine = std::make_unique<QQmlApplicationEngine>();
    QObject::connect(engine.get(), &QQmlEngine::warnings, engine.get(),
                     [this](const QList<QQmlError>& errors)
                     {
                         for (const auto& error : errors)
                         {
                             m_warnings.append(error.toString());
                         }
                     });
    engine->setInitialProperties(
        {{"client", QVariant::fromValue(runtime.models())}, {"desktop", QVariant::fromValue(runtime.state())}});
    engine->loadFromModule("Msa.Ui.Desktop", "DesktopWindow");
    EXPECT_EQ(engine->rootObjects().size(), 1);

    return engine;
}
TEST_F(DesktopRuntimeTest, MissingAndCorruptSettingsKeepWindowAndMusicAvailableWithoutNetwork)
{
    const auto config = options(m_directory);
    for (const bool damaged : {false, true})
    {
        if (damaged)
        {
            writeFile(config.clientSettingsPath, "damaged");
        }
        msa::infra::JsonMusicPreferences::save(config.musicSettingsPath, {"", 0.42});
        msa::desktop::DesktopRuntime runtime{config};
        auto engine = openWindow(runtime);
        ASSERT_EQ(engine->rootObjects().size(), 1);
        runtime.start();
        EXPECT_EQ(runtime.state()->page(), 3);
        EXPECT_FALSE(runtime.state()->canConnect());
        EXPECT_EQ(runtime.models()->connectionState(), "Disconnected");
        EXPECT_EQ(runtime.state()->music()->volumePercent(), 42);
        EXPECT_FALSE(runtime.state()->music()->playing());
        runtime.models()->reconnect();
        settle();
        EXPECT_EQ(runtime.models()->connectionState(), "Disconnected");
        EXPECT_FALSE(runtime.state()->settingsMessage().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().front());
        ASSERT_NE(window, nullptr);
        ASSERT_TRUE(waitFor([window] { return window->isExposed(); }));
        window->resize(window->minimumWidth(), window->minimumHeight());
        settle();
        ASSERT_TRUE(
            window->grabWindow().save(damaged ? "desktop-startup-invalid-preview.png" : "desktop-startup-preview.png"));
        window->close();
        if (damaged)
        {
            QFile file{config.clientSettingsPath};
            ASSERT_TRUE(file.open(QIODevice::ReadOnly));
            EXPECT_EQ(file.readAll(), "damaged");
        }
        else
        {
            EXPECT_FALSE(QFile::exists(config.clientSettingsPath));
        }
    }
}
TEST_F(DesktopRuntimeTest, SaveDoesNotConnectAndExplicitActionUsesSavedParameters)
{
    LocalServer server{"Первый"};
    ASSERT_TRUE(server.m_server.listen(QHostAddress::LocalHost, 0));
    const auto config = options(m_directory);
    msa::infra::JsonClientPreferences::save(config.clientSettingsPath,
                                            preferences(server.m_server.serverPort(), false));
    msa::desktop::DesktopRuntime runtime{config};
    runtime.start();
    settle();
    EXPECT_EQ(server.m_reads, 0);
    ASSERT_TRUE(
        runtime.state()->savePreferences("127.0.0.1", QString::number(server.m_server.serverPort()), kToken, true));
    settle();
    EXPECT_EQ(server.m_reads, 0);
    runtime.state()->connectToServer();
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->online(); }));
    EXPECT_EQ(runtime.models()->tasks()->count(), 1);
    const auto revision = runtime.models()->sessionRevision();
    runtime.models()->reconnect();
    ASSERT_TRUE(waitFor([&runtime, revision]
                        { return runtime.models()->online() && runtime.models()->sessionRevision() > revision; }));
    EXPECT_EQ(runtime.models()->tasks()->count(), 1);
    runtime.stop();
    runtime.stop();
    EXPECT_FALSE(runtime.models()->online());
    EXPECT_TRUE(runtime.models()->tasks()->stale());
}
TEST_F(DesktopRuntimeTest, SwitchingServerDropsOldIdsAndPreservesUnknownWithoutReplay)
{
    LocalServer first{"Прежний сервер"};
    LocalServer second{"Новый сервер"};
    ASSERT_TRUE(first.m_server.listen(QHostAddress::LocalHost, 0));
    ASSERT_TRUE(second.m_server.listen(QHostAddress::LocalHost, 0));
    const auto config = options(m_directory);
    msa::infra::JsonClientPreferences::save(config.clientSettingsPath, preferences(first.m_server.serverPort()));
    msa::desktop::DesktopRuntime runtime{config};
    runtime.start();
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->online() && runtime.models()->chats()->count() == 1; }));
    ASSERT_TRUE(runtime.models()->selectChat("1"));
    first.m_holdWrite = true;
    ASSERT_TRUE(runtime.models()->setTaskDone("1", true));
    ASSERT_TRUE(waitFor([&first] { return first.m_held.has_value(); }));
    ASSERT_TRUE(
        runtime.state()->savePreferences("127.0.0.1", QString::number(second.m_server.serverPort()), kToken, false));
    EXPECT_TRUE(runtime.models()->online());
    EXPECT_EQ(second.m_reads, 0);
    runtime.state()->connectToServer();
    EXPECT_TRUE(runtime.models()->selectedChatIdentifier().isEmpty());
    EXPECT_EQ(runtime.models()->tasks()->count(), 0);
    EXPECT_EQ(runtime.models()->errorEffect(), "unknown");
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->online(); }));
    const auto* tasks = runtime.models()->tasks();
    EXPECT_EQ(tasks->data(tasks->index(0), tasks->roleNames().key("title")).toString(),
              QString::fromUtf8("Новый сервер"));
    settle();
    EXPECT_EQ(first.m_writes, 1);
    EXPECT_EQ(second.m_writes, 0);
    EXPECT_EQ(runtime.models()->errorEffect(), "unknown");
}
TEST_F(DesktopRuntimeTest, ExhaustedCycleWaitsForManualReconnectAndStopCancelsRetry)
{
    QTcpServer reservation;
    ASSERT_TRUE(reservation.listen(QHostAddress::LocalHost, 0));
    const auto port = reservation.serverPort();
    reservation.close();
    const auto config = options(m_directory);
    msa::infra::JsonClientPreferences::save(config.clientSettingsPath, preferences(port));
    msa::desktop::DesktopRuntime runtime{config};
    runtime.start();
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->connectionState() == "Exhausted"; }));
    LocalServer server{"Сервер появился"};
    ASSERT_TRUE(server.m_server.listen(QHostAddress::LocalHost, port));
    settle();
    EXPECT_EQ(server.m_reads, 0);
    runtime.models()->reconnect();
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->online(); }));
    server.m_server.close();
    ASSERT_TRUE(waitFor([&runtime] { return !runtime.models()->online(); }));
    runtime.stop();
    ASSERT_TRUE(server.m_server.listen(QHostAddress::LocalHost, port));
    const auto reads = server.m_reads;
    settle();
    EXPECT_EQ(server.m_reads, reads);
    EXPECT_EQ(runtime.models()->connectionState(), "Disconnected");
}
TEST_F(DesktopRuntimeTest, WindowCreatesTaskThroughRealServerProcessAndRetainsDataAfterDisconnect)
{
    const auto endpoint = QString{"http://127.0.0.1:9"};
    constexpr int kContextBudget = 50000;
    constexpr int kGenerationLimit = 128;
    const QJsonObject serverSettings{{"listen_address", "127.0.0.1"},
                                     {"port", 0},
                                     {"token", kToken},
                                     {"time_zone", "Etc/UTC"},
                                     {"database_path", "assistant.sqlite"},
                                     {"context_max_chars", kContextBudget},
                                     {"check_interval_ms", kQuietMilliseconds},
                                     {"llama", QJsonObject{{"completion_url", endpoint + "/v1/chat/completions"},
                                                           {"model", "test-model"},
                                                           {"api_key", ""},
                                                           {"timeout_ms", kWaitMilliseconds},
                                                           {"max_response_tokens", kGenerationLimit}}},
                                     {"services", QJsonObject{{"timeout_ms", kWaitMilliseconds},
                                                              {"user_agent", "MSA-local-tests"},
                                                              {"nominatim", endpoint + "/search"},
                                                              {"osrm_foot", endpoint + "/foot"},
                                                              {"osrm_bike", endpoint + "/bike"},
                                                              {"osrm_driving", endpoint + "/driving"},
                                                              {"weather", endpoint + "/forecast"},
                                                              {"search", endpoint + "/search"}}}};
    const auto serverPath = m_directory.filePath("server.json");
    writeFile(serverPath, QJsonDocument{serverSettings}.toJson());
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_FORCE_STDERR_LOGGING", "1");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setWorkingDirectory(m_directory.path());
    process.start(QString::fromUtf8(MSA_SERVER_EXECUTABLE), {"--config", serverPath});
    ASSERT_TRUE(process.waitForStarted(kWaitMilliseconds));
    QByteArray output;
    quint16 port{};
    ASSERT_TRUE(waitFor(
        [&process, &output, &port]
        {
            if (port != 0)
            {
                return true;
            }
            output += process.readAll();
            const QRegularExpression pattern{"TCP-[^\r\n]*: ([0-9]+)"};
            const auto match = pattern.match(QString::fromLatin1(output));
            if (match.hasMatch())
            {
                port = match.captured(1).toUShort();
            }
            return port != 0;
        }))
        << output.toStdString();
    const auto config = options(m_directory);
    msa::infra::JsonClientPreferences::save(config.clientSettingsPath, preferences(port));
    msa::desktop::DesktopRuntime runtime{config};
    auto engine = openWindow(runtime);
    ASSERT_EQ(engine->rootObjects().size(), 1);
    runtime.start();
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->online() && runtime.models()->chats()->count() > 0; }));
    runtime.state()->navigate(1);
    auto* planner = runtime.state()->planner();
    planner->newTask();
    planner->setField("title", QString::fromUtf8("Проверить настоящее окно 🙂"));
    ASSERT_TRUE(planner->submit());
    ASSERT_TRUE(waitFor([&runtime, planner] { return !planner->active() && runtime.models()->tasks()->count() == 1; }));
    auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().front());
    ASSERT_NE(window, nullptr);
    settle();
    EXPECT_TRUE(window->grabWindow().save("desktop-runtime-preview.png"));
    process.kill();
    ASSERT_TRUE(process.waitForFinished(kStopMilliseconds));
    ASSERT_TRUE(waitFor([&runtime] { return runtime.models()->connectionState() == "Exhausted"; }));
    EXPECT_EQ(runtime.models()->tasks()->count(), 1);
    EXPECT_TRUE(runtime.models()->tasks()->stale());
    EXPECT_FALSE(runtime.models()->setTaskDone("1", true));
    EXPECT_TRUE(window->isVisible());
    runtime.stop();
    window->close();
}
TEST_F(DesktopRuntimeTest, MainExecutableLoadsBothPluginsAndConnectsWithoutVisibleWindow)
{
    LocalServer server{"Из процесса"};
    ASSERT_TRUE(server.m_server.listen(QHostAddress::LocalHost, 0));
    const auto config = options(m_directory);
    msa::infra::JsonClientPreferences::save(config.clientSettingsPath, preferences(server.m_server.serverPort()));
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    environment.insert("QT_QUICK_BACKEND", "software");
    environment.insert("QT_FORCE_STDERR_LOGGING", "1");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QString::fromUtf8(MSA_DESKTOP_EXECUTABLE),
                  {"--config", config.clientSettingsPath, "--music-config", config.musicSettingsPath});
    ASSERT_TRUE(process.waitForStarted(kWaitMilliseconds));
    const auto ready = waitFor([&server] { return server.m_reads >= 2; });
    const auto output = QString::fromUtf8(process.readAll());
    EXPECT_TRUE(ready) << output.toStdString();
    EXPECT_EQ(process.state(), QProcess::Running) << output.toStdString();
    EXPECT_FALSE(output.contains("TypeError")) << output.toStdString();
    EXPECT_FALSE(output.contains("ReferenceError")) << output.toStdString();
    process.kill();
    ASSERT_TRUE(process.waitForFinished(kStopMilliseconds));
    EXPECT_FALSE(QFile::exists(config.musicSettingsPath));
}
} // namespace
