#include "msa/core/EveningCheckInService.h"

#include <chrono>

namespace msa::core {

EveningCheckInService::EveningCheckInService(const Clock& clock, EveningCheckInRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
}

std::optional<std::string> EveningCheckInService::checkInIfNeeded()
{
    constexpr auto kStartTime = std::chrono::hours{20};
    const auto now = m_clock.now();
    const auto dayStart = std::chrono::floor<std::chrono::days>(now);
    if (now - dayStart < kStartTime) {
        return std::nullopt;
    }
    const auto today = std::chrono::year_month_day{std::chrono::sys_days{dayStart.time_since_epoch()}};
    if (m_repository.lastDate() == today) {
        return std::nullopt;
    }
    const std::string message = "Как прошёл день? 🌙 Если хочешь, расскажи о настроении "
        "и важных событиях — сохраню твой ответ в дневник настроения.";
    m_repository.saveDate(today);

    return message;
}

}  // namespace msa::core

