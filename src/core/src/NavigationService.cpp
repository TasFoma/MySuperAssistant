#include "msa/core/NavigationService.h"

#include "Utf8Text.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>

namespace msa::core
{
namespace
{

constexpr double kEarthRadiusMeters = 6371000.0;
constexpr double kMaximumNonDrivingDistance = 200000.0;
constexpr double kStreetDistanceFactor = 1.3;
constexpr double kWalkingSpeed = 5.0 / 3.6;
constexpr double kCyclingSpeed = 15.0 / 3.6;
constexpr double kDrivingSpeed = 40.0 / 3.6;
constexpr double kMetersPerKilometer = 1000.0;
constexpr double kSecondsPerMinute = 60.0;
constexpr double kMinutesPerHour = 60.0;
constexpr double kDistanceRounding = 10.0;
constexpr int kUmbrellaThreshold = 30;

[[nodiscard]] std::string fixedNumber(double value, int precision = 0)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(precision) << value;

    return stream.str();
}

[[nodiscard]] std::string distanceText(double meters)
{
    if (meters >= kMetersPerKilometer)
    {
        return fixedNumber(meters / kMetersPerKilometer, 1) + " км";
    }

    return fixedNumber(std::round(meters / kDistanceRounding) * kDistanceRounding) + " м";
}

[[nodiscard]] std::string durationText(double seconds)
{
    const auto minutes = std::max(1.0, std::round(seconds / kSecondsPerMinute));
    if (minutes < kMinutesPerHour)
    {
        return fixedNumber(minutes) + " мин";
    }

    return fixedNumber(std::floor(minutes / kMinutesPerHour)) + " ч " +
           fixedNumber(std::fmod(minutes, kMinutesPerHour)) + " мин";
}

[[nodiscard]] std::string directionText(const std::string& modifier)
{
    if (modifier == "left")
    {
        return "налево";
    }
    if (modifier == "right")
    {
        return "направо";
    }
    if (modifier == "slight left")
    {
        return "плавно налево";
    }
    if (modifier == "slight right")
    {
        return "плавно направо";
    }
    if (modifier == "sharp left")
    {
        return "резко налево";
    }
    if (modifier == "sharp right")
    {
        return "резко направо";
    }
    if (modifier == "uturn")
    {
        return "в обратную сторону (разворот)";
    }

    return {};
}

[[nodiscard]] std::string stepText(const RouteStep& step, TravelMode mode)
{
    std::string text;
    if (step.type == "arrive")
    {
        text = "🏁 Вы прибыли в пункт назначения";
        if (step.modifier == "left")
        {
            text += " (слева)";
        }
        if (step.modifier == "right")
        {
            text += " (справа)";
        }

        return text;
    }
    if (step.type == "depart")
    {
        text = step.street.empty() ? "Начните движение" : "Начните движение по " + step.street;
    }
    else if (step.type == "turn" || step.type == "end of road" || step.type == "fork")
    {
        const auto direction = directionText(step.modifier);
        text = direction.empty() ? "Продолжайте движение" : "Поверните " + direction;
        if (!step.street.empty())
        {
            text += " на " + step.street;
        }
    }
    else if (step.type == "roundabout" || step.type == "rotary")
    {
        text = step.exitNumber ? "На круговом движении сверните на " + std::to_string(*step.exitNumber) + "-й съезд"
                               : "Продолжайте движение по круговому движению";
        if (!step.street.empty())
        {
            text += " на " + step.street;
        }
    }
    else
    {
        text = step.street.empty() ? "Продолжайте движение прямо" : "Продолжайте по " + step.street;
    }
    if (step.distanceMeters >= kDistanceRounding)
    {
        text += mode == TravelMode::Foot ? " и пройдите " : " и проедьте ";
        text += distanceText(step.distanceMeters);
    }

    return text;
}

[[nodiscard]] std::string weatherCodeText(int code)
{
    if (code == 0)
    {
        return "ясно ☀️";
    }
    if (code == 1 || code == 2)
    {
        return "переменная облачность ⛅";
    }
    if (code == 3)
    {
        return "пасмурно ☁️";
    }
    if (code == 45 || code == 48)
    {
        return "туман 🌫️";
    }
    if (code == 51 || code == 53 || code == 55 || code == 56 || code == 57)
    {
        return "морось 🌦️";
    }
    if (code == 61 || code == 63 || code == 65 || code == 66 || code == 67)
    {
        return "дождь 🌧️";
    }
    if (code == 71 || code == 73 || code == 75 || code == 77)
    {
        return "снег ❄️";
    }
    if (code == 80 || code == 81 || code == 82)
    {
        return "ливень 🌧️";
    }
    if (code == 85 || code == 86)
    {
        return "снегопад 🌨️";
    }
    if (code == 95 || code == 96 || code == 99)
    {
        return "гроза ⛈️";
    }

    return "состояние погоды не определено";
}

[[nodiscard]] std::string degreeText(double temperature)
{
    const auto rounded = std::round(temperature);

    return (rounded > 0 ? "+" : "") + fixedNumber(rounded == 0 ? 0 : rounded) + "°";
}

} // namespace

bool validCoordinates(GeoCoordinates coordinates)
{
    return std::isfinite(coordinates.latitude) && std::isfinite(coordinates.longitude) && coordinates.latitude >= -90 &&
           coordinates.latitude <= 90 && coordinates.longitude >= -180 && coordinates.longitude <= 180;
}

double geographicDistance(GeoCoordinates origin, GeoCoordinates destination)
{
    constexpr double kRadiansPerDegree = std::numbers::pi / 180.0;
    const auto latitudeDelta = (destination.latitude - origin.latitude) * kRadiansPerDegree;
    const auto longitudeDelta = (destination.longitude - origin.longitude) * kRadiansPerDegree;
    const auto latitudeSine = std::sin(latitudeDelta / 2);
    const auto longitudeSine = std::sin(longitudeDelta / 2);
    const auto haversine =
        std::clamp(latitudeSine * latitudeSine + std::cos(origin.latitude * kRadiansPerDegree) *
                                                     std::cos(destination.latitude * kRadiansPerDegree) *
                                                     longitudeSine * longitudeSine,
                   0.0, 1.0);

    return 2 * kEarthRadiusMeters * std::atan2(std::sqrt(haversine), std::sqrt(1 - haversine));
}

std::string routeText(const RoutePlan& plan, TravelMode mode, const std::string& origin, const std::string& destination)
{
    std::string text;
    if (plan.approximate)
    {
        text = "🗺️ Сервис маршрутов недоступен, поэтому оценка приблизительная:\n\n";
    }
    else
    {
        text = "🗺️ Маршрут: ";
    }
    text += origin + " → " + destination + "\n\n";
    std::size_t number = 1;
    for (const auto& step : plan.steps)
    {
        text += std::to_string(number++) + ". " + stepText(step, mode) + "\n";
    }
    const auto* modeName = mode == TravelMode::Driving ? "на машине"
                           : mode == TravelMode::Bike  ? "на велосипеде"
                                                       : "пешком";
    text += "\n📏 Расстояние: " + std::string{plan.approximate ? "~" : ""} + distanceText(plan.distanceMeters);
    text += "\n⏱️ Время " + std::string{modeName} + ": ~" + durationText(plan.durationSeconds);

    return text;
}

std::string weatherText(const WeatherForecast& forecast)
{
    auto text = "Сейчас " + degreeText(forecast.temperatureCelsius) + " (ощущается как " +
                degreeText(forecast.apparentTemperatureCelsius) + "), " + weatherCodeText(forecast.weatherCode) + ".";
    if (forecast.maximumTemperatureCelsius && forecast.minimumTemperatureCelsius)
    {
        text += " За день от " + degreeText(*forecast.minimumTemperatureCelsius) + " до " +
                degreeText(*forecast.maximumTemperatureCelsius) + ".";
    }
    if (forecast.precipitationProbabilityPercent && *forecast.precipitationProbabilityPercent >= kUmbrellaThreshold)
    {
        text +=
            " Вероятность осадков " + std::to_string(*forecast.precipitationProbabilityPercent) + "% — возьми зонт! ☂️";
    }

    return text;
}

NavigationService::NavigationService(Geocoder& geocoder, RoutePlanner& planner, RouteHistoryService& history) :
    m_geocoder{geocoder}, m_planner{planner}, m_history{history}
{
}

ExternalServiceResult<GeoCoordinates> NavigationService::locate(const std::string& place, GeoCoordinates reference)
{
    const auto result = m_geocoder.search(place);
    if (const auto* error = std::get_if<ExternalServiceError>(&result))
    {
        return *error;
    }
    const auto& candidates = std::get<std::vector<GeocodedPlace>>(result);
    const auto nearest = std::ranges::min_element(
        candidates, [reference](const auto& left, const auto& right)
        { return geographicDistance(reference, left.coordinates) < geographicDistance(reference, right.coordinates); });

    return nearest->coordinates;
}

ExternalServiceResult<RoutePlan> NavigationService::planRoute(GeoCoordinates origin, GeoCoordinates destination,
    TravelMode mode, const std::string& originName, const std::string& destinationName)
{
    const auto distance = geographicDistance(origin, destination);
    if (mode != TravelMode::Driving && distance > kMaximumNonDrivingDistance)
    {
        return ExternalServiceError{ExternalServiceErrorKind::NotFound,
                                    "Ближайшее место дальше 200 км. Уточните адрес, район или город."};
    }
    auto result = m_planner.route(origin, destination, mode);
    if (const auto* error = std::get_if<ExternalServiceError>(&result);
        error && error->kind == ExternalServiceErrorKind::Unavailable)
    {
        const auto speed = mode == TravelMode::Driving ? kDrivingSpeed
                           : mode == TravelMode::Bike  ? kCyclingSpeed
                                                       : kWalkingSpeed;

        result = RoutePlan{.distanceMeters = distance * kStreetDistanceFactor,
                         .durationSeconds = distance * kStreetDistanceFactor / speed,
                         .approximate = true};
    }
    if (const auto* plan = std::get_if<RoutePlan>(&result)) {
        [[maybe_unused]] const auto identifier = m_history.record({.originName = originName,
            .destinationName = destinationName, .origin = origin, .destination = destination, .mode = mode,
            .distanceMeters = plan->distanceMeters, .durationSeconds = plan->durationSeconds,
            .approximate = plan->approximate});
    }

    return result;
}

ExternalServiceResult<RoutePlan> NavigationService::routeFrom(GeoCoordinates origin, const std::string& destination,
                                                              TravelMode mode)
{
    if (!validCoordinates(origin) ||
        (mode != TravelMode::Foot && mode != TravelMode::Bike && mode != TravelMode::Driving))
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Укажите местоположение и способ передвижения."};
    }
    const auto target = locate(destination, origin);
    if (const auto* error = std::get_if<ExternalServiceError>(&target))
    {
        return *error;
    }

    return planRoute(origin, std::get<GeoCoordinates>(target), mode, "Моё местоположение", destination);
}

ExternalServiceResult<RoutePlan> NavigationService::routeBetween(const std::string& origin,
                                                                 const std::string& destination,
                                                                 GeoCoordinates reference, TravelMode mode)
{
    if (!validCoordinates(reference) || text::trim(origin).empty() || text::trim(destination).empty() ||
        (mode != TravelMode::Foot && mode != TravelMode::Bike && mode != TravelMode::Driving))
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Укажите опорное местоположение и способ передвижения."};
    }
    const auto start = locate(origin, reference);
    if (const auto* error = std::get_if<ExternalServiceError>(&start))
    {
        return *error;
    }
    const auto target = locate(destination, reference);
    if (const auto* error = std::get_if<ExternalServiceError>(&target))
    {
        return *error;
    }

    return planRoute(std::get<GeoCoordinates>(start), std::get<GeoCoordinates>(target), mode, origin, destination);
}

} // namespace msa::core
