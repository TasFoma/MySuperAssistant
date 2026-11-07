#include "msa/core/Geocoder.h"

#include "Utf8Text.h"
#include "msa/core/NavigationService.h"

#include <algorithm>

namespace msa::core
{

Geocoder::~Geocoder() = default;

ExternalServiceResult<std::vector<GeocodedPlace>> Geocoder::search(const std::string& place)
{
    const auto query = text::trim(place);
    if (query.empty())
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest, "Укажите название места или адрес."};
    }
    auto result = requestPlaces(query);
    if (const auto* places = std::get_if<std::vector<GeocodedPlace>>(&result))
    {
        if (places->empty())
        {
            return ExternalServiceError{ExternalServiceErrorKind::NotFound,
                                        "Место не найдено. Уточните адрес или город."};
        }
        if (std::ranges::any_of(
                *places, [](const GeocodedPlace& candidate)
                { return !validCoordinates(candidate.coordinates) || text::trim(candidate.name).empty(); }))
        {
            return ExternalServiceError{ExternalServiceErrorKind::InvalidResponse,
                                        "Геокодер вернул неверные данные места."};
        }
    }

    return result;
}

} // namespace msa::core
