#include "InMemoryEveningCheckInRepository.h"

namespace msa::core {

std::optional<std::chrono::year_month_day> InMemoryEveningCheckInRepository::lastDate() const
{
    return m_date;
}

void InMemoryEveningCheckInRepository::saveDate(std::chrono::year_month_day date)
{
    m_date = date;
}

std::optional<PendingEveningCheckIn> InMemoryEveningCheckInRepository::pendingCheckIn() const
{
    return m_pending;
}

void InMemoryEveningCheckInRepository::savePendingCheckIn(std::optional<PendingEveningCheckIn> pending)
{
    m_pending = pending;
}

}  // namespace msa::core

