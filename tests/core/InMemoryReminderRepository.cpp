#include "InMemoryReminderRepository.h"

namespace msa::core {

std::int64_t InMemoryReminderRepository::add(const Reminder& item)
{
    return m_items.add(item);
}

std::optional<Reminder> InMemoryReminderRepository::find(std::int64_t identifier) const
{
    return m_items.find(identifier);
}

std::vector<Reminder> InMemoryReminderRepository::all() const
{
    return m_items.all();
}

bool InMemoryReminderRepository::update(const Reminder& item)
{
    return m_items.update(item);
}

bool InMemoryReminderRepository::remove(std::int64_t identifier)
{
    return m_items.remove(identifier);
}

}  // namespace msa::core

