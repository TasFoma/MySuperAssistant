#pragma once

#include "msa/core/GeocodingCacheRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra
{

/// @brief Постоянные кандидаты геокодинга с атомарной заменой снимка.
class SqliteGeocodingCacheRepository final : public core::GeocodingCacheRepository
{
public:
    /// @brief База со всеми sqliteMigrations должна пережить репозиторий.
    explicit SqliteGeocodingCacheRepository(SqliteDatabase& database);
    /// @brief Возвращает снимок, включая просроченный; срок проверяет CachedGeocoder.
    [[nodiscard]] std::optional<core::GeocodingCacheEntry> find(const std::string& source,
                                                                const std::string& query) const override;
    /// @brief Заменяет весь снимок и его порядок одной транзакцией, в том числе пустой.
    void save(const std::string& source, const std::string& query, const core::GeocodingCacheEntry& entry) override;

private:
    SqliteDatabase& m_database;
};

} // namespace msa::infra
