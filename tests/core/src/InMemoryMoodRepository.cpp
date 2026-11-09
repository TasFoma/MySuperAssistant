#include "InMemoryMoodRepository.h"

namespace msa::core {

std::int64_t InMemoryMoodRepository::add(const MoodEntry& entry)
{
    return m_entries.add(entry);
}

std::vector<MoodEntry> InMemoryMoodRepository::all() const
{
    return m_entries.all();
}

}  // namespace msa::core

