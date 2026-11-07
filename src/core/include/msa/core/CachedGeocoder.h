#pragma once

#include "msa/core/Clock.h"
#include "msa/core/GeocodingCacheRepository.h"

namespace msa::core
{

/// @brief Кэш кандидатов: семь дней для найденных мест, час для пустых ответов.
class CachedGeocoder final : public Geocoder
{
public:
    /// @brief Зависимости должны пережить объект; пустой source отклоняется.
    /// source различает endpoint, язык и прочие параметры поиска; при изменении
    /// области поиска она тоже должна входить в ключ. Выбранная точка не кэшируется.
    CachedGeocoder(Geocoder& source, GeocodingCacheRepository& repository, const Clock& clock, std::string sourceKey);

protected:
    /// @brief Читает пригодный снимок либо один раз вызывает источник вне транзакции.
    [[nodiscard]] ExternalServiceResult<std::vector<GeocodedPlace>> requestPlaces(const std::string& place) override;

private:
    Geocoder& m_source;
    GeocodingCacheRepository& m_repository;
    const Clock& m_clock;
    std::string m_sourceKey{};
};

} // namespace msa::core
