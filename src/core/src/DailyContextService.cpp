#include "msa/core/DailyContextService.h"

#include "msa/core/detail/PlannerText.h"

#include <chrono>
#include <cstddef>
#include <format>

namespace msa::core {

DailyContextService::DailyContextService(const Clock& clock, const CalendarService& calendar,
                                       const TaskService& tasks, const HabitService& habits)
    : m_clock{clock}, m_calendar{calendar}, m_tasks{tasks}, m_habits{habits}
{
}

std::string DailyContextService::dailyContext() const
{
    constexpr std::size_t kMaxTasks = 5;
    const auto now = m_clock.now();
    const auto dayStart = std::chrono::floor<std::chrono::days>(now);
    const auto today = std::chrono::year_month_day{std::chrono::sys_days{dayStart.time_since_epoch()}};
    std::string context = "Сегодня " + text::russianDate(today) + ", время "
        + text::timeText(std::chrono::duration_cast<std::chrono::minutes>(now - dayStart)) + ".\n";
    const auto events = m_calendar.eventsForDate(today);
    if (!events.empty()) {
        context += "События сегодня: ";
        for (std::size_t index = 0; index < events.size(); ++index) {
            const auto& event = events[index];
            context += (index == 0 ? "" : "; ")
                + (event.time ? text::timeText(*event.time) + " — " : "") + event.title;
        }
        context += ".\n";
    }
    std::size_t taskCount = 0;
    for (const auto& task : m_tasks.tasks()) {
        if (task.done) {
            continue;
        }
        context += (taskCount == 0 ? "Открытые задачи: " : "; ") + task.title;
        ++taskCount;
        if (taskCount == kMaxTasks) {
            break;
        }
    }
    if (taskCount != 0) {
        context += ".\n";
    }
    const auto habits = m_habits.habits();
    if (!habits.empty()) {
        context += "Цели пользователя: ";
        for (std::size_t index = 0; index < habits.size(); ++index) {
            const auto& item = habits[index];
            context += (index == 0 ? "" : "; ") + std::format("{}: {}/{}",
                item.habit.name, item.progress, item.habit.target);
            if (!item.habit.unit.empty()) {
                context += " " + item.habit.unit;
            }
        }
        context += ".\n";
    }

    return context;
}

}  // namespace msa::core

