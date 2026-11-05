#include "msa/core/CalendarService.h"

#include <algorithm>
#include <tuple>

namespace msa::core {
namespace {

[[nodiscard]] bool occursOn(const Event& event, std::chrono::year_month_day date)
{
    if (!event.date.ok() || !date.ok() || date < event.date) {
        return false;
    }
    if (date == event.date) {
        return true;
    }
    switch (event.repeat) {
    case EventRepeat::None:
        return false;
    case EventRepeat::Daily:
        return true;
    case EventRepeat::Weekly:
        return std::chrono::weekday{std::chrono::sys_days{event.date}}
               == std::chrono::weekday{std::chrono::sys_days{date}};
    case EventRepeat::Monthly: {
        const auto lastDay = std::chrono::year_month_day_last{
            date.year(), std::chrono::month_day_last{date.month()}}.day();

        return date.day() == std::min(event.date.day(), lastDay);
    }
    }

    return false;
}

}  // namespace

CalendarService::CalendarService(const EventRepository& repository) : m_repository{repository}
{
}

std::vector<Event> CalendarService::eventsForDate(std::chrono::year_month_day date) const
{
    if (!date.ok()) {
        return {};
    }
    std::vector<Event> result;
    for (const auto& event : m_repository.all()) {
        if (occursOn(event, date)) {
            result.push_back(event);
        }
    }
    std::ranges::sort(result, [](const Event& left, const Event& right) {
        return std::tie(left.time, left.identifier) < std::tie(right.time, right.identifier);
    });

    return result;
}

std::vector<std::chrono::day> CalendarService::eventDaysInMonth(std::chrono::year_month month) const
{
    if (!month.ok()) {
        return {};
    }
    const auto events = m_repository.all();
    const auto lastDay = std::chrono::year_month_day_last{
        month.year(), std::chrono::month_day_last{month.month()}}.day();
    std::vector<std::chrono::day> result;
    for (unsigned number = 1; number <= static_cast<unsigned>(lastDay); ++number) {
        const auto date = month / std::chrono::day{number};
        if (std::ranges::any_of(events, [date](const Event& event) { return occursOn(event, date); })) {
            result.push_back(date.day());
        }
    }

    return result;
}

}  // namespace msa::core
