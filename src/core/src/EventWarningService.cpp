#include "msa/core/EventWarningService.h"

#include <array>
#include <optional>

namespace msa::core {
namespace {

constexpr auto kWarningWindow = std::chrono::minutes{60};
constexpr auto kUntimedStart = std::chrono::hours{8};
constexpr auto kDayDuration = std::chrono::hours{24};

}  // namespace

EventWarningService::EventWarningService(const Clock& clock, const CalendarService& calendar,
    EventWarningRepository& repository)
    : m_clock{clock}, m_calendar{calendar}, m_repository{repository}
{
}

std::vector<EventWarning> EventWarningService::checkDue()
{
    const auto now = m_clock.now();
    const auto today = std::chrono::floor<std::chrono::days>(now);
    std::vector<EventWarning> result;
    for (const auto dayStart : std::array{today, today + std::chrono::days{1}}) {
        const auto date = std::chrono::year_month_day{
            std::chrono::sys_days{dayStart.time_since_epoch()}};
        for (const auto& event : m_calendar.eventsForDate(date)) {
            std::optional<EventWarningKind> kind;
            if (event.time) {
                if (*event.time < std::chrono::minutes::zero() || *event.time >= kDayDuration) {
                    continue;
                }
                const auto remaining = dayStart + *event.time - now;
                if (remaining > std::chrono::seconds::zero() && remaining <= kWarningWindow) {
                    kind = EventWarningKind::BeforeStart;
                }
            } else if (now - today >= kUntimedStart) {
                kind = dayStart == today ? EventWarningKind::OnDay : EventWarningKind::DayBefore;
            }
            if (kind && m_repository.markIfNew(event.identifier, date, *kind)) {
                result.push_back(EventWarning{event, date, *kind});
            }
        }
    }

    return result;
}

}  // namespace msa::core

