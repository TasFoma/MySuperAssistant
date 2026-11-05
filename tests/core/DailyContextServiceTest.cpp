#include "FakeClock.h"
#include "InMemoryEventRepository.h"
#include "InMemoryHabitRepository.h"
#include "InMemoryTaskRepository.h"

#include "msa/core/DailyContextService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace msa::core {
namespace {

using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::year;

class DailyContextServiceTest : public ::testing::Test {
protected:
    FakeClock m_clock{local_days{year{2026} / 11 / 6} + hours{9} + minutes{5} + seconds{59}};
    InMemoryEventRepository m_events{};
    InMemoryTaskRepository m_tasks{};
    InMemoryHabitRepository m_habits{};
    CalendarService m_calendar{m_events};
    TaskService m_taskService{m_clock, m_tasks};
    HabitService m_habitService{m_clock, m_habits};
    DailyContextService m_service{m_clock, m_calendar, m_taskService, m_habitService};
};

TEST_F(DailyContextServiceTest, EmptySummaryContainsOnlyCurrentDateAndTimeAndCanBeRepeated)
{
    const std::string expected = "Сегодня пятница, 6 ноября 2026, время 09:05.\n";
    EXPECT_EQ(m_service.dailyContext(), expected);
    EXPECT_EQ(m_service.dailyContext(), expected);
    m_clock.set(local_days{year{2026} / 12 / 31} + hours{23} + minutes{59});
    EXPECT_EQ(m_service.dailyContext(), "Сегодня четверг, 31 декабря 2026, время 23:59.\n");
    m_clock.advance(minutes{1});
    EXPECT_EQ(m_service.dailyContext(), "Сегодня пятница, 1 января 2027, время 00:00.\n");
}

TEST_F(DailyContextServiceTest, IncludesOnlyTodayEventsInCalendarOrderWithRepeats)
{
    const auto date = year{2026} / 11 / 6;
    const auto later = m_events.add(Event{.title = "Позднее", .date = date, .time = minutes{1439}});
    const auto repeated = m_events.add(Event{.title = "Повтор", .date = year{2026} / 10 / 6,
        .time = minutes{545}, .repeat = EventRepeat::Monthly});
    const auto untimed = m_events.add(Event{.title = "Без времени", .date = date});
    const auto other = m_events.add(Event{.title = "Завтра", .date = year{2026} / 11 / 7});
    const std::string expected = "Сегодня пятница, 6 ноября 2026, время 09:05.\n"
        "События сегодня: Без времени; 09:05 — Повтор; 23:59 — Позднее.\n";
    EXPECT_EQ(m_service.dailyContext(), expected);
    EXPECT_EQ(m_service.dailyContext(), expected);
    EXPECT_EQ(m_events.all().size(), 4U);
    EXPECT_EQ(m_events.find(repeated)->date, year{2026} / 10 / 6);
    EXPECT_TRUE(m_events.find(later));
    EXPECT_TRUE(m_events.find(untimed));
    EXPECT_TRUE(m_events.find(other));
}

TEST_F(DailyContextServiceTest, SelectsFiveOpenTasksByPriorityDeadlineAndIdentifier)
{
    const auto done = m_tasks.add(Task{.title = "Готово", .priority = TaskPriority::High, .done = true});
    const auto low = m_tasks.add(Task{.title = "Шестая", .dueDate = year{2020} / 1 / 1,
        .priority = TaskPriority::Low});
    const auto undated = m_tasks.add(Task{.title = "Без срока", .priority = TaskPriority::High});
    const auto later = m_tasks.add(Task{.title = "Позже", .dueDate = year{2027} / 1 / 1,
        .priority = TaskPriority::High});
    const auto first = m_tasks.add(Task{.title = "Первая", .dueDate = year{2026} / 1 / 1,
        .priority = TaskPriority::High});
    const auto second = m_tasks.add(Task{.title = "Вторая", .dueDate = year{2026} / 1 / 1,
        .priority = TaskPriority::High});
    const auto normal = m_tasks.add(Task{.title = "Обычная", .priority = TaskPriority::Normal});
    EXPECT_EQ(m_service.dailyContext(), "Сегодня пятница, 6 ноября 2026, время 09:05.\n"
        "Открытые задачи: Первая; Вторая; Позже; Без срока; Обычная.\n");
    EXPECT_TRUE(m_tasks.find(done)->done);
    for (const auto identifier : {low, undated, later, first, second, normal}) {
        EXPECT_FALSE(m_tasks.find(identifier)->done);
    }
    for (const auto& task : m_tasks.all()) {
        auto finished = task;
        finished.done = true;
        ASSERT_TRUE(m_tasks.update(finished));
    }
    EXPECT_EQ(m_service.dailyContext(), "Сегодня пятница, 6 ноября 2026, время 09:05.\n");
}

TEST_F(DailyContextServiceTest, CombinesSectionsAndReportsDailyAndWeeklyProgressWithoutCapping)
{
    const auto event = m_events.add(Event{.title = "Встреча", .date = year{2026} / 11 / 6});
    const auto task = m_tasks.add(Task{.title = "Задача"});
    const auto daily = m_habits.add(Habit{.name = "Вода", .target = 2.0, .unit = "л"});
    const auto weekly = m_habits.add(Habit{.name = "Чтение", .target = 10.0, .unit = "стр",
        .period = HabitPeriod::Week});
    const auto empty = m_habits.add(Habit{.name = "Без единицы", .target = 1.0});
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = daily,
        .date = year{2026} / 11 / 6, .value = 2.5}));
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = daily,
        .date = year{2026} / 11 / 5, .value = 7.0}));
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = weekly,
        .date = year{2026} / 11 / 2, .value = 4.0}));
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = weekly,
        .date = year{2026} / 11 / 6, .value = 8.5}));
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = weekly,
        .date = year{2026} / 11 / 7, .value = 100.0}));
    EXPECT_EQ(m_service.dailyContext(), "Сегодня пятница, 6 ноября 2026, время 09:05.\n"
        "События сегодня: Встреча.\nОткрытые задачи: Задача.\n"
        "Цели пользователя: Вода: 2.5/2 л; Чтение: 12.5/10 стр; Без единицы: 0/1.\n");
    EXPECT_EQ(m_habits.logs(daily).size(), 2U);
    EXPECT_EQ(m_habits.logs(weekly).size(), 3U);
    EXPECT_TRUE(m_habits.logs(empty).empty());
    EXPECT_TRUE(m_events.find(event));
    EXPECT_FALSE(m_tasks.find(task)->done);
    m_clock.set(local_days{year{2026} / 11 / 9});
    EXPECT_EQ(m_service.dailyContext(), "Сегодня понедельник, 9 ноября 2026, время 00:00.\n"
        "Открытые задачи: Задача.\n"
        "Цели пользователя: Вода: 0/2 л; Чтение: 0/10 стр; Без единицы: 0/1.\n");
}

TEST_F(DailyContextServiceTest, PropagatesDependencyFailureWithoutReturningPartialContext)
{
    const auto identifier = m_habits.add(Habit{.name = "Ошибка"});
    ASSERT_TRUE(m_habits.addLog(HabitLogEntry{.habitIdentifier = identifier,
        .date = year{2026} / 11 / 6, .value = -1.0}));
    EXPECT_THROW(static_cast<void>(m_service.dailyContext()), std::runtime_error);
    EXPECT_EQ(m_habits.logs(identifier).size(), 1U);
}

}  // namespace
}  // namespace msa::core

