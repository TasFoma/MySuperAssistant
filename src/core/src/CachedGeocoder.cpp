#include "msa/core/CachedGeocoder.h"

#include "Utf8Text.h"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace msa::core
{

CachedGeocoder::CachedGeocoder(Geocoder& source, GeocodingCacheRepository& repository, const Clock& clock,
                               std::string sourceKey) :
    m_source{source}, m_repository{repository}, m_clock{clock}, m_sourceKey{std::move(sourceKey)}
{
    if (text::trim(m_sourceKey).empty())
    {
        throw std::invalid_argument{"Укажите ключ поставщика геокодинга."};
    }
}

ExternalServiceResult<std::vector<GeocodedPlace>> CachedGeocoder::requestPlaces(const std::string& place)
{
    constexpr auto kPositiveLifetime = std::chrono::days{7};
    constexpr auto kNegativeLifetime = std::chrono::hours{1};
    const auto entry = m_repository.find(m_sourceKey, place);
    const auto now = m_clock.now();
    if (entry && entry->storedAt <= now)
    {
        const auto lifetime =
            entry->places.empty() ? std::chrono::seconds{kNegativeLifetime} : std::chrono::seconds{kPositiveLifetime};
        // Беззнаковая разность не переполняется при крайних значениях часов.
        const auto age = static_cast<std::uint64_t>(now.time_since_epoch().count()) -
                         static_cast<std::uint64_t>(entry->storedAt.time_since_epoch().count());
        if (age < static_cast<std::uint64_t>(lifetime.count()))
        {
            return entry->places;
        }
    }
    auto result = m_source.search(place);
    if (const auto* candidates = std::get_if<std::vector<GeocodedPlace>>(&result))
    {
        m_repository.save(m_sourceKey, place, {m_clock.now(), *candidates});
    }
    else if (std::get<ExternalServiceError>(result).kind == ExternalServiceErrorKind::NotFound)
    {
        // Geocoder::search превращает пустой успешный список в NotFound.
        m_repository.save(m_sourceKey, place, {m_clock.now(), {}});
    }

    return result;
}

} // namespace msa::core
