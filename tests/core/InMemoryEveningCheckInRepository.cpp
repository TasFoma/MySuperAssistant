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

}  // namespace msa::core

