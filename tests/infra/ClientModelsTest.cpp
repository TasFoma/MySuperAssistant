#include "../core/FakeClock.h"
#include "DesktopClientController.h"
#include "ProtocolTestClient.h"
#include "msa/infra/ClientProtocol.h"
#include "msa/infra/ProtocolPayload.h"
#include "msa/infra/TcpServer.h"
#include "msa/ui/ClientModels.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <vector>

namespace msa::infra::test
{
namespace
{
using namespace std::chrono_literals;
using Kind = core::ClientReadKind;
constexpr auto kDate = std::chrono::year{2026} / 11 / 9;
constexpr auto kMoment = std::chrono::local_days{kDate} + 12h;
constexpr auto kMaximumId = std::numeric_limits<std::int64_t>::max();

core::Task task(std::int64_t identifier = 1, const std::string& title = "Полная задача 🙂")
{
    return {.identifier = identifier, .title = title, .priority = core::TaskPriority::High, .createdAt = kMoment};
}

core::ClientSnapshot data()
{
    return {.serverTime = kMoment,
            .timeZone = "Etc/UTC",
            .mainChatIdentifier = 1,
            .tasks = {task(kMaximumId)},
            .reminders = {core::PlaceReminder{
                .identifier = 2, .title = "Место", .place = "Магазин", .coordinates = core::GeoCoordinates{0, 0}}},
            .habits = {{core::Habit{.identifier = 3, .name = "Чтение", .target = 1.5, .unit = "стр"}, 3.5}},
            .eventsToday = {
                {core::Event{.identifier = 4, .title = "Серия", .date = kDate, .repeat = core::EventRepeat::Daily},
                 kDate}}};
}

QJsonObject snapshot()
{
    const auto value = data();
    return {{"server_time", ProtocolPayload::localDateTime(value.serverTime)},
            {"time_zone", "Etc/UTC"},
            {"main_chat_id", "1"},
            {"tasks", QJsonArray{ProtocolPayload::object(value.tasks.front())}},
            {"reminders", QJsonArray{ProtocolPayload::object(value.reminders.front())}},
            {"habits", QJsonArray{ProtocolPayload::object(value.habits.front())}},
            {"events_today", QJsonArray{ProtocolPayload::occurrence(value.eventsToday.front().event, kDate)}}};
}

QVariant role(const ui::ClientListModel& model, int row, const QByteArray& name)
{
    const auto roles = model.roleNames();
    const auto found = roles.key(name, -1);
    EXPECT_GT(found, Qt::UserRole);
    return model.data(model.index(row), found);
}

void online(ui::ClientModels& models)
{
    models.setConnectionState(core::ClientConnectionState::Ready);
    ASSERT_TRUE(models.applyResult(data(), models.sessionRevision(), models.viewRevision()));
    ASSERT_TRUE(models.applyResult(core::ClientChatList{{core::Chat{1, "Основной"}, core::Chat{2, "Второй"}}, 1},
                                   models.sessionRevision(), models.viewRevision()));
}

core::ChatMessage message(std::int64_t identifier, std::int64_t chatId, const std::string& text = "Текст\n🙂")
{
    return {identifier, chatId, core::MessageRole::User, text, kMoment};
}

TEST(ClientProtocolTest, DecodesCompleteSnapshotWithoutPrecisionLossAndShowsNullableAndZeroValues)
{
    const auto decoded = ClientProtocol::decode({}, snapshot());
    const auto& value = std::get<core::ClientSnapshot>(decoded);
    EXPECT_EQ(value.tasks.front().identifier, kMaximumId);
    EXPECT_FALSE(value.tasks.front().dueDate);
    EXPECT_EQ(value.tasks.front().title, "Полная задача 🙂");
    ASSERT_TRUE(std::get<core::PlaceReminder>(value.reminders.front()).coordinates);
    EXPECT_DOUBLE_EQ(value.habits.front().progress, 3.5);
    ui::ClientModels models;
    models.setConnectionState(core::ClientConnectionState::Ready);
    ASSERT_TRUE(models.applyResult(decoded, models.sessionRevision(), models.viewRevision()));
    EXPECT_EQ(role(*models.tasks(), 0, "identifier").toString(), "9223372036854775807");
    EXPECT_FALSE(role(*models.tasks(), 0, "dueDate").isValid());
    EXPECT_DOUBLE_EQ(role(*models.reminders(), 0, "latitude").toDouble(), 0);
    EXPECT_TRUE(role(*models.reminders(), 0, "hasCoordinates").toBool());
    EXPECT_EQ(models.serverTime(), "2026-11-09T12:00:00");
    EXPECT_DOUBLE_EQ(role(*models.habits(), 0, "progress").toDouble(), 3.5);
}

TEST(ClientProtocolTest, RejectsMalformedNestedFieldsDuplicatesAndUnknownDataBeforeReplacingModels)
{
    const auto valid = snapshot();
    const auto original = valid["tasks"].toArray().first().toObject();
    for (const auto& replacement : std::vector<QJsonObject>{{{"id", 1}},
                                                            {{"done", 1}},
                                                            {{"priority", "Unknown"}},
                                                            {{"created_at", "2026-02-30T12:00:00"}},
                                                            {{"due_date", ""}},
                                                            {{"title", QJsonValue::Null}},
                                                            {{"unexpected", true}}})
    {
        auto invalid = original;
        for (const auto& key : replacement.keys())
        {
            invalid[key] = replacement.value(key);
        }
        auto root = valid;
        root["tasks"] = QJsonArray{invalid};
        EXPECT_THROW(static_cast<void>(ClientProtocol::decode({}, root)), std::invalid_argument);
    }
    for (const auto& key : original.keys())
    {
        auto invalid = original;
        invalid.remove(key);
        auto root = valid;
        root["tasks"] = QJsonArray{invalid};
        EXPECT_THROW(static_cast<void>(ClientProtocol::decode({}, root)), std::invalid_argument);
    }
    auto duplicate = valid;
    duplicate["tasks"] = QJsonArray{original, original};
    EXPECT_THROW(static_cast<void>(ClientProtocol::decode({}, duplicate)), std::invalid_argument);
    ui::ClientModels models;
    online(models);
    EXPECT_EQ(models.tasks()->count(), 1);
    EXPECT_EQ(role(*models.tasks(), 0, "title").toString(), "Полная задача 🙂");
}

TEST(ClientProtocolTest, ParsesAllCollectionKindsAndPreservesProfileSourcesAndRouteStatistics)
{
    const auto chats = ClientProtocol::decode(
        {.kind = Kind::Chats},
        {{"chats", QJsonArray{QJsonObject{{"id", "1"}, {"title", "Основной"}}}}, {"main_chat_id", "1"}});
    EXPECT_EQ(std::get<core::ClientChatList>(chats).chats.size(), 1U);
    core::UserProfileFact fact{.text = "Полный факт",
                               .tags = {"тег"},
                               .alwaysInclude = true,
                               .identifier = 7,
                               .source = core::ProfileFactSource{1, 6, std::nullopt},
                               .replacesFactIdentifier = 5};
    const auto profile =
        ClientProtocol::decode({.kind = Kind::Profile}, {{"facts", QJsonArray{ProtocolPayload::object(fact)}}});
    EXPECT_EQ(std::get<core::ClientProfile>(profile).facts.front(), fact);
    const core::MoodEntry mood{8, kMoment, core::MoodScore::Negative, "  Полная заметка\n  "};
    const auto diary =
        ClientProtocol::decode({.kind = Kind::Moods}, {{"entries", QJsonArray{ProtocolPayload::object(mood)}}});
    EXPECT_EQ(std::get<core::ClientMoods>(diary).entries.front().note, mood.note);
    const auto calendar = ClientProtocol::decode({.kind = Kind::CalendarDay, .date = kDate},
                                                 {{"date", "2026-11-09"}, {"events", snapshot()["events_today"]}});
    EXPECT_EQ(std::get<core::ClientCalendarDay>(calendar).events.front().event.date, kDate);
    const auto month = ClientProtocol::decode({.kind = Kind::CalendarMonth, .month = std::chrono::year{2026} / 11},
                                              {{"year", 2026}, {"month", 11}, {"days", QJsonArray{9, 30}}});
    EXPECT_EQ(std::get<core::ClientCalendarMonth>(month).days, (std::vector<unsigned>{9, 30}));
    const core::RouteCalculation route{.identifier = 9,
                                       .createdAt = kMoment,
                                       .originName = "Начало",
                                       .destinationName = "Конец",
                                       .origin = {0, 0},
                                       .destination = {1, 1},
                                       .mode = core::TravelMode::Driving,
                                       .distanceMeters = 10,
                                       .durationSeconds = 20,
                                       .approximate = true};
    const core::RouteCalculationStatistics stats{2, 1, 50, 60};
    const auto routes =
        ClientProtocol::decode({.kind = Kind::Routes}, {{"recent", QJsonArray{ProtocolPayload::object(route)}},
                                                        {"statistics", ProtocolPayload::object(stats)}});
    EXPECT_TRUE(std::get<core::ClientRoutes>(routes).recent.front().approximate);
    EXPECT_EQ(std::get<core::ClientRoutes>(routes).statistics.routedCount, 2U);
    ui::ClientModels models;
    online(models);
    ASSERT_TRUE(models.applyResult(routes, models.sessionRevision(), models.viewRevision()));
    EXPECT_EQ(models.routeStatistics().value("routedCount").toULongLong(), 2U);
}

TEST(ClientProtocolTest, RejectsWrongChatNonAdvancingCursorOrderAndEmptyMorePage)
{
    const core::ClientReadRequest request{.kind = Kind::Messages, .chatIdentifier = 1, .afterMessageIdentifier = 2};
    const QJsonObject valid{{"chat_id", "1"},
                            {"messages", QJsonArray{ProtocolPayload::object(message(3, 1))}},
                            {"next_after_message_id", "3"},
                            {"has_more", true}};
    EXPECT_EQ(std::get<core::ClientMessagePage>(ClientProtocol::decode(request, valid)).messages.size(), 1U);
    for (const auto& change : std::vector<QJsonObject>{
             {{"chat_id", "2"}},
             {{"next_after_message_id", "2"}},
             {{"messages", QJsonArray{ProtocolPayload::object(message(3, 2))}}},
             {{"messages", QJsonArray{ProtocolPayload::object(message(3, 1)), ProtocolPayload::object(message(3, 1))}}},
             {{"messages", QJsonArray{}}, {"next_after_message_id", "2"}}})
    {
        auto value = valid;
        for (const auto& key : change.keys())
        {
            value[key] = change[key];
        }
        EXPECT_THROW(static_cast<void>(ClientProtocol::decode(request, value)), std::invalid_argument);
    }
}

TEST(ClientProtocolTest, EncodesTypedActionsAndRejectsMismatchedAcknowledgement)
{
    const core::ClientAction done = core::ClientSetTaskDone{kMaximumId, true};
    const auto wire = ClientProtocol::request(done);
    EXPECT_EQ(wire.method, "task.set_done");
    EXPECT_EQ(wire.params.value("task_id"), "9223372036854775807");
    EXPECT_NO_THROW(ClientProtocol::validateActionResult(done, {{"task_id", "9223372036854775807"}}));
    EXPECT_THROW(ClientProtocol::validateActionResult(done, {{"task_id", "1"}}), std::invalid_argument);
    const auto send =
        ClientProtocol::request(core::ClientAction{core::ClientSendMessage{1, "Полный текст", std::nullopt}});
    EXPECT_TRUE(send.params.value("search_query").isNull());
    EXPECT_EQ(send.params.value("text"), "Полный текст");
}

TEST(ClientModelsTest, ListRejectsWrongTypesAndDuplicatesWithoutNotificationsOrPartialChanges)
{
    ui::ClientListModel model{ui::ClientListKind::Tasks};
    int changes{0};
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&changes]() { ++changes; });
    ASSERT_TRUE(model.replaceRows({task()}));
    EXPECT_FALSE(model.replaceRows({task(), task()}));
    EXPECT_FALSE(model.replaceRows({core::Chat{1, "Чужой тип"}}));
    EXPECT_EQ(model.count(), 1);
    EXPECT_EQ(changes, 1);
    EXPECT_FALSE(model.data(QModelIndex{}, Qt::UserRole + 1).isValid());
    EXPECT_EQ(model.rowCount(model.index(0)), 0);
}

TEST(ClientModelsTest, NonvisualQmlReadsModelsAndTracksOfflineProperties)
{
    ui::ClientModels models;
    online(models);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("clientModels", &models);
    QQmlComponent component{&engine};
    component.setData(R"(import QtQml
        QtObject {
            property int taskCount: clientModels.tasks.count
            property bool canWrite: clientModels.serverActionsEnabled
            property bool taskDataStale: clientModels.tasks.stale
        })",
                      QUrl{});
    std::unique_ptr<QObject> object{component.create()};
    ASSERT_NE(object, nullptr) << component.errorString().toStdString();
    EXPECT_EQ(object->property("taskCount").toInt(), 1);
    EXPECT_TRUE(object->property("canWrite").toBool());
    models.setConnectionState(core::ClientConnectionState::Disconnected);
    EXPECT_FALSE(object->property("canWrite").toBool());
    EXPECT_TRUE(object->property("taskDataStale").toBool());
    EXPECT_EQ(object->property("taskCount").toInt(), 1);
}

TEST(ClientModelsTest, OfflineRetainsRowsAndChatCacheButEmitsNoServerActionsOrReadRequests)
{
    ui::ClientModels models;
    online(models);
    ASSERT_TRUE(models.selectChat("1"));
    ASSERT_TRUE(models.applyResult(core::ClientMessagePage{1, {}, {message(10, 1)}, 10, false},
                                   models.sessionRevision(), models.viewRevision()));
    int actions{0};
    int reads{0};
    int reconnects{0};
    QObject::connect(&models, &ui::ClientModels::actionRequested, &models,
                     [&actions](const core::ClientAction&) { ++actions; });
    QObject::connect(&models, &ui::ClientModels::readRequested, &models,
                     [&reads](core::ClientReadRequest, quint64, quint64) { ++reads; });
    QObject::connect(&models, &ui::ClientModels::reconnectRequested, &models, [&reconnects]() { ++reconnects; });
    models.setConnectionState(core::ClientConnectionState::Exhausted, 5);
    models.invalidateSession();
    EXPECT_FALSE(models.online());
    EXPECT_TRUE(models.statusText().contains("Переподключиться"));
    EXPECT_EQ(models.tasks()->count(), 1);
    EXPECT_EQ(models.messages()->count(), 1);
    EXPECT_TRUE(models.tasks()->stale());
    EXPECT_FALSE(models.setTaskDone("9223372036854775807", true));
    EXPECT_FALSE(models.sendMessage("Не отправлять"));
    EXPECT_FALSE(models.retryMessage("10"));
    EXPECT_FALSE(models.createChat());
    models.refresh();
    EXPECT_FALSE(models.loadMoreMessages());
    EXPECT_TRUE(models.selectChat("2"));
    EXPECT_TRUE(models.selectChat("1"));
    EXPECT_EQ(models.messages()->count(), 1);
    EXPECT_TRUE(models.messages()->stale());
    EXPECT_EQ(actions, 0);
    EXPECT_EQ(reads, 0);
    models.reconnect();
    EXPECT_EQ(reconnects, 1);
    models.setError("Результат записи неизвестен", "unknown");
    models.setError("Фоновое чтение не удалось", "none");
    EXPECT_EQ(models.errorEffect(), "unknown");
    EXPECT_EQ(models.errorText(), "Результат записи неизвестен");
}

TEST(ClientModelsTest, MessagePagesRespectSessionSelectionAndCursorWithoutDuplication)
{
    ui::ClientModels models;
    online(models);
    ASSERT_TRUE(models.selectChat("1"));
    const auto session = models.sessionRevision();
    const auto oldView = models.viewRevision();
    ASSERT_TRUE(models.applyResult(core::ClientMessagePage{1, {}, {message(10, 1)}, 10, true}, session, oldView));
    const core::ClientMessagePage second{1, 10, {message(11, 1)}, 11, false};
    ASSERT_TRUE(models.applyResult(second, session, oldView));
    EXPECT_FALSE(models.applyResult(second, session, oldView));
    EXPECT_EQ(models.messages()->count(), 2);
    ASSERT_TRUE(models.selectChat("2"));
    EXPECT_FALSE(models.applyResult(core::ClientMessagePage{1, {}, {message(99, 1)}, 99, false}, session, oldView));
    ASSERT_TRUE(models.selectChat("1"));
    EXPECT_FALSE(models.applyResult(core::ClientMessagePage{1, {}, {message(99, 1)}, 99, false}, session, oldView));
    EXPECT_EQ(models.messages()->count(), 2);
    models.invalidateSession();
    EXPECT_FALSE(models.applyResult(data(), session, models.viewRevision()));
    models.setConnectionState(core::ClientConnectionState::Ready);
    ASSERT_TRUE(models.applyResult(data(), models.sessionRevision(), models.viewRevision()));
    EXPECT_EQ(models.selectedChatIdentifier(), "1");
    EXPECT_TRUE(models.messages()->stale());
}

class ClientCompositionTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void connectReady();
    core::FakeClock m_clock{kMoment};
    TcpServer m_server{"composition-token", "Etc/UTC", m_clock};
    std::unique_ptr<TcpClient> m_client{};
    ui::ClientModels m_models{};
    std::unique_ptr<desktop::DesktopClientController> m_controller{};
    QJsonObject m_snapshot{snapshot()};
    bool m_profileFailure{false};
    bool m_badActionAck{false};
    bool m_holdSnapshots{false};
    std::vector<std::pair<quint64, ProtocolRequest>> m_heldSnapshots{};
    int m_profileReads{0};
    int m_writes{0};
};

void ClientCompositionTest::SetUp()
{
    ASSERT_TRUE(m_server.listen(QHostAddress::LocalHost, 0));
    QObject::connect(
        &m_server, &TcpServer::requestReceived, &m_server,
        [this](quint64 connection, ProtocolRequest request)
        {
            QJsonObject result;
            if (request.method == "get_all" && m_holdSnapshots)
            {
                m_heldSnapshots.emplace_back(connection, request);
                return;
            }
            if (request.method == "get_all")
            {
                result = m_snapshot;
            }
            else if (request.method == "chat.list")
            {
                result = {{"chats", QJsonArray{QJsonObject{{"id", "1"}, {"title", "Основной"}}}},
                          {"main_chat_id", "1"}};
            }
            else if (request.method == "profile.get")
            {
                ++m_profileReads;
                if (m_profileFailure)
                {
                    static_cast<void>(m_server.sendResponse(
                        connection,
                        ProtocolFailure{request.requestId, "response_too_large", "Профиль не помещается", "none", {}}));
                    return;
                }
                result = {{"facts", QJsonArray{}}};
            }
            else if (request.method == "mood.history")
            {
                result = {{"entries", QJsonArray{}}};
            }
            else if (request.method == "route.history")
            {
                result = {{"recent", QJsonArray{}},
                          {"statistics", ProtocolPayload::object(core::RouteCalculationStatistics{})}};
            }
            else if (request.method == "task.set_done")
            {
                ++m_writes;
                auto tasks = m_snapshot["tasks"].toArray();
                auto item = tasks.first().toObject();
                item["done"] = request.params["done"];
                tasks[0] = item;
                m_snapshot["tasks"] = tasks;
                result = {{"task_id", m_badActionAck ? QJsonValue{"999"} : request.params["task_id"]}};
            }
            else
            {
                ADD_FAILURE() << request.method.toStdString();
            }
            static_cast<void>(m_server.sendResponse(connection, ProtocolSuccess{request.requestId, result}));
        });
    m_client = std::make_unique<TcpClient>(
        TcpClientSettings{"127.0.0.1", m_server.serverPort(), "composition-token", 500ms, 1000ms, 5ms, 20ms});
    m_controller = std::make_unique<desktop::DesktopClientController>(*m_client, m_models);
}

void ClientCompositionTest::connectReady()
{
    m_client->connectToServer();
    ASSERT_TRUE(
        waitForTest([this]() { return m_models.online() && m_models.chats()->count() == 1 && m_profileReads >= 1; }));
}

TEST_F(ClientCompositionTest, TypedNetworkUpdatesModelsAndAcknowledgedActionRefreshesData)
{
    connectReady();
    EXPECT_EQ(m_models.tasks()->count(), 1);
    ASSERT_TRUE(m_models.setTaskDone("9223372036854775807", true));
    ASSERT_TRUE(waitForTest([this]() { return role(*m_models.tasks(), 0, "done").toBool(); }));
    EXPECT_EQ(m_writes, 1);
    m_client->disconnectFromServer();
    EXPECT_FALSE(m_models.online());
    EXPECT_EQ(m_models.tasks()->count(), 1);
    EXPECT_TRUE(m_models.tasks()->stale());
    EXPECT_FALSE(m_models.setTaskDone("9223372036854775807", false));
    m_models.reconnect();
    ASSERT_TRUE(waitForTest([this]() { return m_models.online(); }));
    EXPECT_EQ(m_writes, 1);
    EXPECT_TRUE(role(*m_models.tasks(), 0, "done").toBool());
}

TEST_F(ClientCompositionTest, MalformedNestedSnapshotDisconnectsWithoutReplacingLastGoodRows)
{
    connectReady();
    auto item = m_snapshot["tasks"].toArray().first().toObject();
    item["done"] = "invalid";
    m_snapshot["tasks"] = QJsonArray{item};
    m_models.refresh();
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Disconnected; }));
    EXPECT_EQ(m_models.tasks()->count(), 1);
    EXPECT_FALSE(role(*m_models.tasks(), 0, "done").toBool());
    EXPECT_TRUE(m_models.tasks()->stale());
    EXPECT_FALSE(m_models.online());
}

TEST_F(ClientCompositionTest, InvalidWriteAcknowledgementRemainsUnknownAndIsNotReplayed)
{
    connectReady();
    m_badActionAck = true;
    ASSERT_TRUE(m_models.setTaskDone("9223372036854775807", true));
    ASSERT_TRUE(waitForTest([this]() { return m_client->state() == TcpClientState::Disconnected; }));
    EXPECT_EQ(m_models.errorEffect(), "unknown");
    EXPECT_EQ(m_writes, 1);
    EXPECT_FALSE(role(*m_models.tasks(), 0, "done").toBool());
    EXPECT_TRUE(m_models.tasks()->stale());
    m_models.reconnect();
    ASSERT_TRUE(waitForTest([this]() { return m_models.online(); }));
    EXPECT_EQ(m_models.errorEffect(), "unknown");
    EXPECT_EQ(m_writes, 1);
    EXPECT_TRUE(role(*m_models.tasks(), 0, "done").toBool());
}

TEST_F(ClientCompositionTest, FailedReadIsNotRetriedInAnUnboundedRefreshLoop)
{
    connectReady();
    ASSERT_TRUE(waitForTest([this]() { return !m_models.profile()->stale(); }));
    m_profileFailure = true;
    m_models.refresh();
    ASSERT_TRUE(waitForTest([this]() { return m_profileReads == 2; }));
    bool done{false};
    QTimer::singleShot(100, m_client.get(), [&done]() { done = true; });
    ASSERT_TRUE(waitForTest([&done]() { return done; }));
    EXPECT_EQ(m_profileReads, 2);
    EXPECT_TRUE(m_models.profile()->stale());
    EXPECT_TRUE(m_models.online());
    EXPECT_EQ(m_models.errorText(), "Профиль не помещается");
}

TEST_F(ClientCompositionTest, OlderSnapshotResponseCannotOverwriteNewerRefresh)
{
    connectReady();
    m_holdSnapshots = true;
    m_models.refresh();
    m_models.refresh();
    ASSERT_TRUE(waitForTest([this]() { return m_heldSnapshots.size() == 2; }));
    auto fresh = m_snapshot;
    auto item = fresh["tasks"].toArray().first().toObject();
    item["title"] = "Более новый снимок";
    fresh["tasks"] = QJsonArray{item};
    const auto newer = m_heldSnapshots[1];
    const auto older = m_heldSnapshots[0];
    ASSERT_TRUE(m_server.sendResponse(newer.first, ProtocolSuccess{newer.second.requestId, fresh}));
    ASSERT_TRUE(m_server.sendResponse(older.first, ProtocolSuccess{older.second.requestId, m_snapshot}));
    ASSERT_TRUE(
        waitForTest([this]() { return role(*m_models.tasks(), 0, "title").toString() == "Более новый снимок"; }));
    bool done{false};
    QTimer::singleShot(100, m_client.get(), [&done]() { done = true; });
    ASSERT_TRUE(waitForTest([&done]() { return done; }));
    EXPECT_EQ(role(*m_models.tasks(), 0, "title").toString(), "Более новый снимок");
}
} // namespace
} // namespace msa::infra::test
