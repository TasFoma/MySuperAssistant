#include "InMemoryEventWarningRepository.h"

namespace msa::core {

bool InMemoryEventWarningRepository::markIfNew(std::int64_t identifier,
    std::chrono::year_month_day occurrenceDate, EventWarningKind kind)
{
    return m_marks.emplace(identifier, occurrenceDate, kind).second;
}

}  // namespace msa::core

