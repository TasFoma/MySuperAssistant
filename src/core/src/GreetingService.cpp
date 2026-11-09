#include "msa/core/GreetingService.h"

#include "msa/core/detail/PlannerText.h"

#include <algorithm>
#include <chrono>
#include <cstddef>

namespace msa::core {
namespace {

constexpr int kMorningHour = 8;
constexpr int kDayHour = 12;
constexpr int kEveningHour = 18;
constexpr std::size_t kMaxTasks = 5;

}  // namespace

GreetingService::GreetingService(const Clock& clock, const CalendarService& calendar,
                                 const TaskRepository& tasks, GreetingRepository& greetings)
    : m_clock{clock}, m_calendar{calendar}, m_tasks{tasks}, m_greetings{greetings}
{
}

std::optional<std::string> GreetingService::greetingIfNeeded()
{
    const auto now = m_clock.now();
    const auto dayStart = std::chrono::floor<std::chrono::days>(now);
    const auto today = std::chrono::year_month_day{std::chrono::sys_days{dayStart.time_since_epoch()}};
    if (m_greetings.lastDate() == today) {
        return std::nullopt;
    }
    const auto hour = std::chrono::duration_cast<std::chrono::hours>(now - dayStart).count();
    const std::string salutation = hour < kMorningHour ? "Доброй ночи"
                                   : hour < kDayHour ? "Доброе утро"
                                   : hour < kEveningHour ? "Добрый день" : "Добрый вечер";
    std::string message = salutation + "! Сегодня " + text::russianDate(today) + ".";
    const auto events = m_calendar.eventsForDate(today);
    if (!events.empty()) {
        message += "\n\n📅 Планы на сегодня:";
        for (const auto& event : events) {
            message += "\n• " + (event.time ? text::timeText(*event.time) + " — " : "") + event.title;
        }
    }
    auto tasks = m_tasks.all();
    std::erase_if(tasks, [](const Task& task) { return task.done; });
    std::ranges::sort(tasks, [](const Task& left, const Task& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        if (left.dueDate.has_value() != right.dueDate.has_value()) {
            return left.dueDate.has_value();
        }
        if (left.dueDate != right.dueDate) {
            return left.dueDate < right.dueDate;
        }

        return left.identifier < right.identifier;
    });
    if (!tasks.empty()) {
        message += "\n\n✅ Задачи:";
        const auto count = std::min(kMaxTasks, tasks.size());
        for (std::size_t index = 0; index < count; ++index) {
            message += "\n• " + tasks[index].title;
        }
    }
    if (events.empty() && tasks.empty()) {
        message += " Планов пока нет — отличный день, чтобы что-то задумать. Чем займёмся?";
    }
    m_greetings.saveDate(today);

    return message;
}

}  // namespace msa::core
