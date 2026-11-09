#include "msa/infra/ClientProtocol.h"
#include "msa/ui/PlannerEditor.h"

#include <gtest/gtest.h>

#include <QJsonObject>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono;
constexpr auto kDate = year{2026} / 11 / 9;
constexpr auto kMoment = local_days{kDate} + hours{19};
constexpr std::int64_t kLargeId = 9007199254740993LL;

QVariantMap field(const msa::ui::PlannerEditor& editor, const QString& key)
{
    for (const auto& item : editor.fields())
    {
        const auto result = item.toMap();
        if (result.value("key").toString() == key)
        {
            return result;
        }
    }
    return {};
}

TEST(PlannerProtocolTest, EncodesEveryFormAndChecksExactAcknowledgement)
{
    using namespace msa::core;
    struct Case
    {
        ClientPlannerAction action;
        QString method;
        QJsonObject result;
    };
    const std::vector<Case> cases{
        {ClientSaveTask{{}, "Задача", {}, TaskPriority::High}, "task.create", {{"task_id", "1"}}},
        {ClientSaveTask{kLargeId, "Изменить", kDate, TaskPriority::Low},
         "task.update",
         {{"task_id", QString::number(kLargeId)}}},
        {ClientSaveEvent{{}, "Событие", kDate, {}, "", EventRepeat::None}, "event.create", {{"event_id", "2"}}},
        {ClientSaveEvent{kLargeId, "Серия", kDate, minutes{60}, "<b>текст</b>", EventRepeat::Monthly},
         "event.update",
         {{"event_id", QString::number(kLargeId)}}},
        {ClientCreateHabit{"Чтение", 1.5, "", HabitPeriod::Week}, "habit.create", {{"habit_id", "3"}}},
        {ClientLogHabit{kLargeId, 2.5}, "habit.log", {{"log_id", "4"}}},
        {ClientCreateTimedReminder{"Время", kMoment}, "reminder.create", {{"reminder_id", "5"}}},
        {ClientCreateRecurringReminder{"Повтор", minutes{90}}, "reminder.create", {{"reminder_id", "6"}}},
        {ClientCreatePlaceReminder{"Место", "Магазин"}, "reminder.create", {{"reminder_id", "7"}}},
        {ClientDeleteItem{ClientItemKind::Task, kLargeId}, "task.delete", {{"deleted", true}}},
        {ClientDeleteItem{ClientItemKind::Event, kLargeId}, "event.delete", {{"deleted", true}}},
        {ClientDeleteItem{ClientItemKind::Habit, kLargeId}, "habit.delete", {{"deleted", true}}},
        {ClientDeleteItem{ClientItemKind::Reminder, kLargeId}, "reminder.delete", {{"deleted", true}}}};
    for (const auto& item : cases)
    {
        const auto wire = msa::infra::ClientProtocol::request(ClientAction{item.action});
        EXPECT_EQ(wire.method, item.method);
        EXPECT_NO_THROW(msa::infra::ClientProtocol::validateActionResult(ClientAction{item.action}, item.result));
        auto extra = item.result;
        extra.insert("extra", true);
        EXPECT_THROW(msa::infra::ClientProtocol::validateActionResult(ClientAction{item.action}, extra),
                     std::invalid_argument);
        EXPECT_THROW(msa::infra::ClientProtocol::validateActionResult(ClientAction{item.action}, {}),
                     std::invalid_argument);
    }
    const auto update = msa::infra::ClientProtocol::request(ClientAction{cases.at(1).action}).params;
    EXPECT_EQ(update.value("task_id").toString(), QString::number(kLargeId));
    EXPECT_FALSE(update.contains("done"));
    EXPECT_FALSE(update.contains("created_at"));
    EXPECT_EQ(update.value("priority"), "Low");
    EXPECT_TRUE(
        msa::infra::ClientProtocol::request(ClientAction{cases.front().action}).params.value("due_date").isNull());
    const auto recurring = msa::infra::ClientProtocol::request(ClientAction{cases.at(7).action}).params;
    EXPECT_EQ(recurring.size(), 3);
    EXPECT_FALSE(recurring.contains("due_at"));
    const auto place = msa::infra::ClientProtocol::request(ClientAction{cases.at(8).action}).params;
    EXPECT_EQ(place.size(), 3);
    EXPECT_FALSE(place.contains("coordinates"));
    EXPECT_THROW(
        msa::infra::ClientProtocol::validateActionResult(ClientAction{cases.at(1).action}, {{"task_id", "12"}}),
        std::invalid_argument);
    EXPECT_THROW(
        msa::infra::ClientProtocol::validateActionResult(ClientAction{cases.back().action}, {{"deleted", false}}),
        std::invalid_argument);
}

TEST(PlannerProtocolTest, RejectsInvalidTypedParametersBeforeSending)
{
    using namespace msa::core;
    const std::vector<ClientPlannerAction> invalid{
        ClientSaveTask{{}, "", {}, TaskPriority::Normal},
        ClientSaveEvent{{}, "Дата", year{2026} / 2 / 30, {}, "", EventRepeat::None},
        ClientCreateHabit{"Имя", std::numeric_limits<double>::infinity(), "", HabitPeriod::Day},
        ClientLogHabit{1, -1},
        ClientDeleteItem{ClientItemKind::Task, 0},
        ClientCreateRecurringReminder{"Повтор", minutes{0}}};
    for (const auto& action : invalid)
    {
        EXPECT_THROW(static_cast<void>(msa::infra::ClientProtocol::request(ClientAction{action})),
                     std::invalid_argument);
    }
}

class PlannerEditorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
        msa::core::ClientSnapshot snapshot;
        snapshot.mainChatIdentifier = 1;
        snapshot.timeZone = "Asia/Novosibirsk";
        snapshot.serverTime = kMoment;
        snapshot.tasks.push_back({kLargeId, "Исходная задача", {}, msa::core::TaskPriority::High, true, kMoment});
        snapshot.eventsToday.push_back(
            {{2, "Серия", year{2026} / 10 / 1, {}, "Описание", msa::core::EventRepeat::Daily}, kDate});
        snapshot.habits.push_back({{3, "Чтение", 10, "стр.", msa::core::HabitPeriod::Day}, 2});
        snapshot.reminders.push_back(msa::core::TimedReminder{4, "Напоминание", kMoment});
        ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
        QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_editor,
                         [this](const msa::core::ClientAction& action)
                         { m_actions.push_back(std::get<msa::core::ClientPlannerAction>(action)); });
    }
    msa::ui::ClientModels m_models{};
    msa::ui::PlannerEditor m_editor{m_models};
    std::vector<msa::core::ClientPlannerAction> m_actions{};
};

TEST_F(PlannerEditorTest, UpdatesExactTaskWithoutOptimisticChangesAndClosesOnlyOnMatchingSuccess)
{
    m_editor.editTask(QString::number(kLargeId));
    EXPECT_EQ(field(m_editor, "title").value("value").toString(), "Исходная задача");
    EXPECT_EQ(field(m_editor, "priority").value("optionIndex").toInt(), 2);
    m_editor.setField("title", "  Новый текст 🙂  ");
    m_editor.setField("dueDate", "2026-11-09");
    ASSERT_TRUE(m_editor.submit());
    ASSERT_EQ(m_actions.size(), 1U);
    const auto& action = std::get<msa::core::ClientSaveTask>(m_actions.front());
    EXPECT_EQ(action.identifier, kLargeId);
    EXPECT_EQ(action.title, "  Новый текст 🙂  ");
    EXPECT_EQ(action.dueDate, kDate);
    EXPECT_TRUE(m_editor.active());
    EXPECT_TRUE(m_editor.pending());
    EXPECT_FALSE(m_editor.submit());
    EXPECT_EQ(m_models.tasks()->rowByIdentifier(QString::number(kLargeId)).value("title").toString(),
              "Исходная задача");
    EXPECT_TRUE(m_models.tasks()->rowByIdentifier(QString::number(kLargeId)).value("done").toBool());
    m_models.finishPlannerAction(msa::core::ClientCreateHabit{"Другая", 1, "", msa::core::HabitPeriod::Day}, true);
    EXPECT_TRUE(m_editor.active());
    m_models.finishPlannerAction(m_actions.front(), true);
    EXPECT_FALSE(m_editor.active());
}

TEST_F(PlannerEditorTest, EditsOriginalEventDateAndConfirmsDeletingWholeSeries)
{
    m_editor.editEvent("2");
    EXPECT_EQ(field(m_editor, "date").value("value").toString(), "2026-10-01");
    EXPECT_TRUE(field(m_editor, "time").value("value").toString().isEmpty());
    m_editor.setField("time", "23:59");
    ASSERT_TRUE(m_editor.submit());
    const auto& action = std::get<msa::core::ClientSaveEvent>(m_actions.back());
    EXPECT_EQ(action.date, year{2026} / 10 / 1);
    EXPECT_EQ(action.time, minutes{1439});
    m_models.finishPlannerAction(m_actions.back(), true);
    m_editor.deleteEvent("2");
    EXPECT_TRUE(m_editor.destructive());
    EXPECT_TRUE(m_editor.explanation().contains("вся серия"));
    m_editor.cancel();
    EXPECT_EQ(m_actions.size(), 1U);
    m_editor.deleteEvent("2");
    ASSERT_TRUE(m_editor.submit());
    EXPECT_EQ(std::get<msa::core::ClientDeleteItem>(m_actions.back()).kind, msa::core::ClientItemKind::Event);
}

TEST_F(PlannerEditorTest, SupportsFractionalHabitGoalAndLogsWithoutClamping)
{
    m_editor.newHabit();
    m_editor.setField("name", "Привычка");
    m_editor.setField("target", "1,5");
    m_editor.setChoice("period", 1);
    ASSERT_TRUE(m_editor.submit());
    const auto& create = std::get<msa::core::ClientCreateHabit>(m_actions.back());
    EXPECT_DOUBLE_EQ(create.target, 1.5);
    EXPECT_TRUE(create.unit.empty());
    EXPECT_EQ(create.period, msa::core::HabitPeriod::Week);
    m_models.finishPlannerAction(m_actions.back(), true);
    m_editor.recordHabit("3");
    m_editor.setField("value", "25.5");
    ASSERT_TRUE(m_editor.submit());
    EXPECT_DOUBLE_EQ(std::get<msa::core::ClientLogHabit>(m_actions.back()).value, 25.5);
    EXPECT_EQ(m_models.habits()->rowByIdentifier("3").value("progress").toDouble(), 2);
}

TEST_F(PlannerEditorTest, ReminderKindPreservesDraftAndSendsOnlySelectedFields)
{
    m_editor.newReminder();
    m_editor.setField("title", "Текст");
    m_editor.setField("dueAt", "2026-11-09T19:00:00");
    m_editor.setChoice("kind", 1);
    EXPECT_FALSE(m_editor.fieldVisibility().value("dueAt").toBool());
    m_editor.setField("interval", "90");
    m_editor.setChoice("kind", 2);
    m_editor.setField("place", "Адрес");
    m_editor.setChoice("kind", 0);
    EXPECT_EQ(field(m_editor, "dueAt").value("value").toString(), "2026-11-09T19:00:00");
    ASSERT_TRUE(m_editor.submit());
    EXPECT_EQ(std::get<msa::core::ClientCreateTimedReminder>(m_actions.back()).dueAt, kMoment);
    m_models.finishPlannerAction(m_actions.back(), true);
    m_editor.newReminder();
    m_editor.setField("title", "Повтор");
    m_editor.setChoice("kind", 1);
    ASSERT_TRUE(m_editor.submit());
    EXPECT_EQ(std::get<msa::core::ClientCreateRecurringReminder>(m_actions.back()).interval, minutes{60});
    m_models.finishPlannerAction(m_actions.back(), true);
    m_editor.newReminder();
    m_editor.setField("title", "Место");
    m_editor.setChoice("kind", 2);
    m_editor.setField("place", "Адрес");
    ASSERT_TRUE(m_editor.submit());
    EXPECT_EQ(std::get<msa::core::ClientCreatePlaceReminder>(m_actions.back()).place, "Адрес");
}

TEST_F(PlannerEditorTest, InvalidInputAndCancelledDeleteNeverProduceWrites)
{
    m_editor.newTask();
    EXPECT_FALSE(m_editor.submit());
    m_editor.setField("title", "Задача");
    m_editor.setField("dueDate", "2026-02-30");
    EXPECT_FALSE(m_editor.submit());
    m_editor.newEvent();
    m_editor.setField("title", "Дата");
    m_editor.setField("date", "2026-11-09");
    m_editor.setField("time", "24:00");
    EXPECT_FALSE(m_editor.submit());
    m_editor.newHabit();
    m_editor.setField("name", "Имя");
    for (const auto* number : {"0", "-1", "nan", "inf", "1,2.3"})
    {
        m_editor.setField("target", number);
        EXPECT_FALSE(m_editor.submit());
    }
    m_editor.newReminder();
    m_editor.setField("title", "Текст");
    m_editor.setChoice("kind", 1);
    m_editor.setField("interval", "1.5");
    EXPECT_FALSE(m_editor.submit());
    m_editor.deleteTask(QString::number(kLargeId));
    m_editor.cancel();
    m_editor.deleteHabit("3");
    m_editor.cancel();
    m_editor.deleteReminder("4");
    m_editor.cancel();
    EXPECT_TRUE(m_actions.empty());
}

TEST_F(PlannerEditorTest, RejectionKeepsDraftAndUnknownResultRequiresReopening)
{
    m_editor.newTask();
    m_editor.setField("title", "Черновик");
    ASSERT_TRUE(m_editor.submit());
    m_models.setError("Сервер отказал");
    m_models.finishPlannerAction(m_actions.back(), false);
    EXPECT_TRUE(m_editor.active());
    EXPECT_TRUE(m_editor.canSubmit());
    EXPECT_EQ(field(m_editor, "title").value("value").toString(), "Черновик");
    ASSERT_TRUE(m_editor.submit());
    m_models.setError("Итог другого действия неизвестен", "unknown");
    m_models.finishPlannerAction(m_actions.back(), false, "Сервер занят", "none");
    EXPECT_TRUE(m_editor.canSubmit());
    EXPECT_EQ(m_editor.errorText(), "Сервер занят");
    EXPECT_EQ(m_models.errorEffect(), "unknown");
    ASSERT_TRUE(m_editor.submit());
    m_models.setError("Итог неизвестен", "unknown");
    m_models.finishPlannerAction(m_actions.back(), false);
    EXPECT_FALSE(m_editor.canSubmit());
    EXPECT_FALSE(m_editor.submit());
    EXPECT_EQ(m_actions.size(), 3U);
    EXPECT_EQ(field(m_editor, "title").value("value").toString(), "Черновик");
}

TEST_F(PlannerEditorTest, SessionLossDoesNotReplayOrApplyLateAcknowledgement)
{
    m_editor.newTask();
    m_editor.setField("title", "Черновик");
    ASSERT_TRUE(m_editor.submit());
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    EXPECT_FALSE(m_editor.pending());
    EXPECT_FALSE(m_editor.canSubmit());
    EXPECT_TRUE(m_editor.errorText().contains("неизвестен"));
    m_models.finishPlannerAction(m_actions.front(), true);
    EXPECT_TRUE(m_editor.active());
    EXPECT_EQ(m_actions.size(), 1U);
    EXPECT_EQ(field(m_editor, "title").value("value").toString(), "Черновик");
}
} // namespace
