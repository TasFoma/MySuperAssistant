#include "msa/core/WeatherProvider.h"

#include "msa/core/NavigationService.h"

#include <cmath>

namespace msa::core
{

WeatherProvider::~WeatherProvider() = default;

ExternalServiceResult<WeatherForecast> WeatherProvider::forecast(GeoCoordinates coordinates)
{
    if (!validCoordinates(coordinates))
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Укажите корректные координаты для прогноза погоды."};
    }
    auto result = requestForecast(coordinates);
    if (const auto* weather = std::get_if<WeatherForecast>(&result))
    {
        if (!std::isfinite(weather->temperatureCelsius) || !std::isfinite(weather->apparentTemperatureCelsius) ||
            !std::isfinite(weather->windSpeedKilometersPerHour) || weather->windSpeedKilometersPerHour < 0 ||
            weather->weatherCode < 0 ||
            weather->maximumTemperatureCelsius.has_value() != weather->minimumTemperatureCelsius.has_value() ||
            (weather->maximumTemperatureCelsius &&
             (!std::isfinite(*weather->maximumTemperatureCelsius) ||
              !std::isfinite(*weather->minimumTemperatureCelsius) ||
              *weather->minimumTemperatureCelsius > *weather->maximumTemperatureCelsius)) ||
            (weather->precipitationProbabilityPercent &&
             (*weather->precipitationProbabilityPercent < 0 || *weather->precipitationProbabilityPercent > 100)))
        {
            return ExternalServiceError{ExternalServiceErrorKind::InvalidResponse,
                                        "Сервис погоды вернул неверные данные."};
        }
    }

    return result;
}

} // namespace msa::core
