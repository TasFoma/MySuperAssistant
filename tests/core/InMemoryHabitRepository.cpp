#include "InMemoryHabitRepository.h"

namespace msa::core {

std::int64_t InMemoryHabitRepository::add(const Habit& item)
{
    return m_items.add(item);
}

std::optional<Habit> InMemoryHabitRepository::find(std::int64_t identifier) const
{
    return m_items.find(identifier);
}

std::vector<Habit> InMemoryHabitRepository::all() const
{
    return m_items.all();
}

bool InMemoryHabitRepository::update(const Habit& item)
{
    return m_items.update(item);
}

bool InMemoryHabitRepository::remove(std::int64_t identifier)
{
    if (!m_items.remove(identifier)) {
        return false;
    }
    for (const auto& entry : logs(identifier)) {
        static_cast<void>(m_logs.remove(entry.identifier));
    }

    return true;
}

std::optional<std::int64_t> InMemoryHabitRepository::addLog(const HabitLogEntry& entry)
{
    if (!m_items.find(entry.habitIdentifier)) {
        return std::nullopt;
    }

    return m_logs.add(entry);
}

std::vector<HabitLogEntry> InMemoryHabitRepository::logs(std::int64_t habitIdentifier) const
{
    std::vector<HabitLogEntry> result;
    for (const auto& entry : m_logs.all()) {
        if (entry.habitIdentifier == habitIdentifier) {
            result.push_back(entry);
        }
    }

    return result;
}

}  // namespace msa::core

