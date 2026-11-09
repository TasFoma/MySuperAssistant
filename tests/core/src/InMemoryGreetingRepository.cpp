#include "InMemoryGreetingRepository.h"

namespace msa::core {

std::optional<std::chrono::year_month_day> InMemoryGreetingRepository::lastDate() const
{
    return m_date;
}

void InMemoryGreetingRepository::saveDate(std::chrono::year_month_day date)
{
    m_date = date;
}

}  // namespace msa::core
