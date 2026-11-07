#include "InMemoryGeocodingCacheRepository.h"

#include <stdexcept>

namespace msa::core
{

std::optional<GeocodingCacheEntry> InMemoryGeocodingCacheRepository::find(const std::string& source,
                                                                          const std::string& query) const
{
    const auto item = m_items.find({source, query});
    if (item == m_items.end())
    {
        return std::nullopt;
    }

    return item->second;
}

void InMemoryGeocodingCacheRepository::save(const std::string& source, const std::string& query,
                                            const GeocodingCacheEntry& entry)
{
    if (failSave)
    {
        throw std::runtime_error{"Отказ записи кэша."};
    }
    m_items[{source, query}] = entry;
}

} // namespace msa::core
