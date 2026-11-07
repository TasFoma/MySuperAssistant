#pragma once

#include "msa/core/GeocodingCacheRepository.h"

#include <map>
#include <utility>

namespace msa::core
{

/// @brief Тестовый кэш кандидатов без файлов.
class InMemoryGeocodingCacheRepository final : public GeocodingCacheRepository
{
public:
    /// @brief Возвращает копию или nullopt.
    [[nodiscard]] std::optional<GeocodingCacheEntry> find(const std::string& source,
                                                          const std::string& query) const override;
    /// @brief Заменяет снимок целиком.
    void save(const std::string& source, const std::string& query, const GeocodingCacheEntry& entry) override;
    /// @brief Имитирует техническую ошибку записи.
    bool failSave{false};

private:
    std::map<std::pair<std::string, std::string>, GeocodingCacheEntry> m_items{};
};

} // namespace msa::core
