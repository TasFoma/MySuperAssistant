#include "InMemoryEventRepository.h"

namespace msa::core {

std::int64_t InMemoryEventRepository::add(const Event& item)
{
    return m_items.add(item);
}

std::optional<Event> InMemoryEventRepository::find(std::int64_t identifier) const
{
    return m_items.find(identifier);
}

std::vector<Event> InMemoryEventRepository::all() const
{
    return m_items.all();
}

bool InMemoryEventRepository::update(const Event& item)
{
    return m_items.update(item);
}

bool InMemoryEventRepository::remove(std::int64_t identifier)
{
    return m_items.remove(identifier);
}

}  // namespace msa::core

