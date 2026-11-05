#include "msa/core/GreetingService.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>

namespace msa::core {
namespace {

constexpr int kMorningHour = 8;
constexpr int kDayHour = 12;
constexpr int kEveningHour = 18;
constexpr int kMinutesPerHour = 60;
constexpr std::size_t kMaxTasks = 5;

[[nodiscard]] std::string russianDate(std::chrono::year_month_day date)
{
    static constexpr std::array<std::string_view, 7> kDays{
        "понедельник", "вторник", "среда", "четверг", "пятница", "суббота", "воскресенье"};
    static constexpr std::array<std::string_view, 12> kMonths{
        "января", "февраля", "марта", "апреля", "мая", "июня",
        "июля", "августа", "сентября", "октября", "ноября", "декабря"};
    const auto weekday = std::chrono::weekday{std::chrono::sys_days{date}}.iso_encoding();

    return std::string{kDays[weekday - 1]} + ", " + std::to_string(static_cast<unsigned>(date.day()))
           + " " + std::string{kMonths[static_cast<unsigned>(date.month()) - 1]}
           + " " + std::to_string(static_cast<int>(date.year()));
}

[[nodiscard]] std::string timeText(std::chrono::minutes time)
{
    const auto hour = time.count() / kMinutesPerHour;
    const auto minute = time.count() % kMinutesPerHour;

    return (hour < 10 ? "0" : "") + std::to_string(hour) + ":"
           + (minute < 10 ? "0" : "") + std::to_string(minute);
}

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
    std::string message = salutation + "! Сегодня " + russianDate(today) + ".";
    const auto events = m_calendar.eventsForDate(today);
    if (!events.empty()) {
        message += "\n\n📅 Планы на сегодня:";
        for (const auto& event : events) {
            message += "\n• " + (event.time ? timeText(*event.time) + " — " : "") + event.title;
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
