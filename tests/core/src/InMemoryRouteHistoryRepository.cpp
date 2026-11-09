#include "InMemoryRouteHistoryRepository.h"

#include <stdexcept>

namespace msa::core
{

std::int64_t InMemoryRouteHistoryRepository::add(const RouteCalculation& calculation)
{
    if (failAdd)
    {
        throw std::runtime_error{"Отказ записи истории."};
    }
    auto item = calculation;
    item.identifier = m_nextIdentifier++;
    m_items.push_back(item);

    return item.identifier;
}

std::vector<RouteCalculation> InMemoryRouteHistoryRepository::all() const
{
    return m_items;
}

} // namespace msa::core
