#include "InMemoryTaskRepository.h"

namespace msa::core {

std::int64_t InMemoryTaskRepository::add(const Task& item)
{
    return m_items.add(item);
}

std::optional<Task> InMemoryTaskRepository::find(std::int64_t identifier) const
{
    return m_items.find(identifier);
}

std::vector<Task> InMemoryTaskRepository::all() const
{
    return m_items.all();
}

bool InMemoryTaskRepository::update(const Task& item)
{
    return m_items.update(item);
}

bool InMemoryTaskRepository::remove(std::int64_t identifier)
{
    return m_items.remove(identifier);
}

}  // namespace msa::core

