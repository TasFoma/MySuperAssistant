#include "FakeClock.h"
#include "InMemoryRouteHistoryRepository.h"

#include "msa/core/NavigationService.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace msa::core
{
namespace
{

class FakeGeocoder final : public Geocoder
{
public:
    std::map<std::string, ExternalServiceResult<std::vector<GeocodedPlace>>> results{};
    std::vector<std::string> queries{};

protected:
    [[nodiscard]] ExternalServiceResult<std::vector<GeocodedPlace>> requestPlaces(const std::string& place) override;
};

ExternalServiceResult<std::vector<GeocodedPlace>> FakeGeocoder::requestPlaces(const std::string& place)
{
    queries.push_back(place);
    const auto found = results.find(place);
    if (found == results.end())
    {
        return std::vector<GeocodedPlace>{};
    }

    return found->second;
}

class FakeRoutePlanner final : public RoutePlanner
{
public:
    ExternalServiceResult<RoutePlan> result{RoutePlan{1000, 600, {{"depart", {}, "Улица", 1000}, {"arrive"}}}};
    int calls{};
    GeoCoordinates lastOrigin{};
    GeoCoordinates lastDestination{};
    TravelMode lastMode{TravelMode::Foot};

protected:
    [[nodiscard]] ExternalServiceResult<RoutePlan> requestRoute(GeoCoordinates origin, GeoCoordinates destination,
                                                                TravelMode mode) override;
};

ExternalServiceResult<RoutePlan> FakeRoutePlanner::requestRoute(GeoCoordinates origin, GeoCoordinates destination,
                                                                TravelMode mode)
{
    ++calls;
    lastOrigin = origin;
    lastDestination = destination;
    lastMode = mode;

    return result;
}

class FakeWeatherProvider final : public WeatherProvider
{
public:
    ExternalServiceResult<WeatherForecast> result{WeatherForecast{}};
    int calls{};

protected:
    [[nodiscard]] ExternalServiceResult<WeatherForecast> requestForecast(GeoCoordinates coordinates) override;
};

ExternalServiceResult<WeatherForecast> FakeWeatherProvider::requestForecast([[maybe_unused]] GeoCoordinates coordinates)
{
    ++calls;

    return result;
}

template <typename ValueType>
void expectError(const ExternalServiceResult<ValueType>& result, ExternalServiceErrorKind kind)
{
    ASSERT_TRUE(std::holds_alternative<ExternalServiceError>(result));
    EXPECT_EQ(std::get<ExternalServiceError>(result).kind, kind);
    EXPECT_FALSE(std::get<ExternalServiceError>(result).reason.empty());
}

TEST(NavigationServiceTest, ValidatesCoordinatesAndComputesSphericalDistanceAcrossDateLine)
{
    EXPECT_TRUE(validCoordinates({0, 0}));
    EXPECT_TRUE(validCoordinates({90, -180}));
    EXPECT_FALSE(validCoordinates({91, 0}));
    EXPECT_FALSE(validCoordinates({0, 181}));
    EXPECT_FALSE(validCoordinates({std::numeric_limits<double>::quiet_NaN(), 0}));
    EXPECT_FALSE(validCoordinates({0, std::numeric_limits<double>::infinity()}));
    EXPECT_DOUBLE_EQ(geographicDistance({0, 0}, {0, 0}), 0);
    EXPECT_NEAR(geographicDistance({0, 179.9}, {0, -179.9}), 22239, 2);
    EXPECT_NEAR(geographicDistance({0, 0}, {0, 180}), 20015087, 2);
}

TEST(NavigationServiceTest, TrimsPlaceAndRejectsInvalidCandidatesWithoutInventingCoordinates)
{
    FakeGeocoder geocoder;
    expectError(geocoder.search(" \n "), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(geocoder.queries.empty());
    expectError(geocoder.search("  Место  "), ExternalServiceErrorKind::NotFound);
    EXPECT_EQ(geocoder.queries.back(), "Место");
    geocoder.results["Место"] = std::vector<GeocodedPlace>{{{91, 0}, "Адрес"}};
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::InvalidResponse);
    geocoder.results["Место"] = std::vector<GeocodedPlace>{{{0, 0}, " "}};
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::InvalidResponse);
}

TEST(NavigationServiceTest, ChoosesNearestCandidateAcrossDateLineKeepingInputOrigin)
{
    FakeGeocoder geocoder;
    geocoder.results["Место"] =
        std::vector<GeocodedPlace>{{{0, 170}, "Дальше"}, {{0, -179.9}, "Ближе"}, {{0, -179.9}, "Равный"}};
    FakeRoutePlanner planner;
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryRouteHistoryRepository repository;
    RouteHistoryService history{repository, clock};
    NavigationService service{geocoder, planner, history};
    EXPECT_TRUE(std::holds_alternative<RoutePlan>(service.routeFrom({0, 179.9}, "Место", TravelMode::Bike)));
    EXPECT_EQ(planner.calls, 1);
    EXPECT_DOUBLE_EQ(planner.lastOrigin.longitude, 179.9);
    EXPECT_DOUBLE_EQ(planner.lastDestination.longitude, -179.9);
    EXPECT_EQ(planner.lastMode, TravelMode::Bike);
}

TEST(NavigationServiceTest, ResolvesBothPlaceNamesAroundExplicitReference)
{
    FakeGeocoder geocoder;
    geocoder.results["Начало"] = std::vector<GeocodedPlace>{{{0, 0}, "Начало"}};
    geocoder.results["Конец"] = std::vector<GeocodedPlace>{{{0, 1}, "Конец"}};
    FakeRoutePlanner planner;
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryRouteHistoryRepository repository;
    RouteHistoryService history{repository, clock};
    NavigationService service{geocoder, planner, history};
    EXPECT_TRUE(std::holds_alternative<RoutePlan>(service.routeBetween("Начало", "Конец", {0, 0}, TravelMode::Foot)));
    EXPECT_EQ(geocoder.queries, (std::vector<std::string>{"Начало", "Конец"}));
    EXPECT_DOUBLE_EQ(planner.lastOrigin.longitude, 0);
    EXPECT_DOUBLE_EQ(planner.lastDestination.longitude, 1);
}

TEST(NavigationServiceTest, StopsOnInvalidLocationModeOrGeocodingFailure)
{
    FakeGeocoder geocoder;
    FakeRoutePlanner planner;
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryRouteHistoryRepository repository;
    RouteHistoryService history{repository, clock};
    NavigationService service{geocoder, planner, history};
    expectError(service.routeFrom({100, 0}, "Место", TravelMode::Foot), ExternalServiceErrorKind::InvalidRequest);
    expectError(service.routeFrom({0, 0}, "Место", static_cast<TravelMode>(99)),
                ExternalServiceErrorKind::InvalidRequest);
    expectError(service.routeBetween("Начало", " \n ", {0, 0}, TravelMode::Foot),
                ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(geocoder.queries.empty());
    expectError(service.routeFrom({0, 0}, "Место", TravelMode::Foot), ExternalServiceErrorKind::NotFound);
    expectError(service.routeBetween("Начало", "Конец", {0, 0}, TravelMode::Foot), ExternalServiceErrorKind::NotFound);
    EXPECT_EQ(planner.calls, 0);
}

TEST(NavigationServiceTest, RejectsDistantFootAndBikeButAllowsDriving)
{
    FakeGeocoder geocoder;
    geocoder.results["Место"] = std::vector<GeocodedPlace>{{{0, 2}, "Далеко"}};
    FakeRoutePlanner planner;
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryRouteHistoryRepository repository;
    RouteHistoryService history{repository, clock};
    NavigationService service{geocoder, planner, history};
    expectError(service.routeFrom({0, 0}, "Место", TravelMode::Foot), ExternalServiceErrorKind::NotFound);
    expectError(service.routeFrom({0, 0}, "Место", TravelMode::Bike), ExternalServiceErrorKind::NotFound);
    EXPECT_EQ(planner.calls, 0);
    EXPECT_TRUE(std::holds_alternative<RoutePlan>(service.routeFrom({0, 0}, "Место", TravelMode::Driving)));
}

TEST(NavigationServiceTest, EstimatesOnlyUnavailableRoutesWithModeSpeedAndNoInventedSteps)
{
    FakeGeocoder geocoder;
    geocoder.results["Место"] = std::vector<GeocodedPlace>{{{0, 0.01}, "Рядом"}};
    FakeRoutePlanner planner;
    planner.result = ExternalServiceError{ExternalServiceErrorKind::Unavailable, "Недоступен"};
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryRouteHistoryRepository repository;
    RouteHistoryService history{repository, clock};
    NavigationService service{geocoder, planner, history};
    const auto distance = geographicDistance({0, 0}, {0, 0.01}) * 1.3;
    const std::vector<std::pair<TravelMode, double>> modes{
        {TravelMode::Foot, 5}, {TravelMode::Bike, 15}, {TravelMode::Driving, 40}};
    for (const auto& [mode, speed] : modes)
    {
        const auto result = service.routeFrom({0, 0}, "Место", mode);
        ASSERT_TRUE(std::holds_alternative<RoutePlan>(result));
        const auto& plan = std::get<RoutePlan>(result);
        EXPECT_TRUE(plan.approximate);
        EXPECT_TRUE(plan.steps.empty());
        EXPECT_DOUBLE_EQ(plan.distanceMeters, distance);
        EXPECT_NEAR(plan.durationSeconds, distance / (speed / 3.6), 0.001);
        EXPECT_NE(routeText(plan, mode, "Откуда", "Куда").find("оценка приблизительная"), std::string::npos);
    }
    for (const auto kind : {ExternalServiceErrorKind::NotFound, ExternalServiceErrorKind::InvalidResponse})
    {
        planner.result = ExternalServiceError{kind, "Ошибка"};
        expectError(service.routeFrom({0, 0}, "Место", TravelMode::Foot), kind);
    }
}

TEST(NavigationServiceTest, ValidatesCompleteRouteAndWeatherResults)
{
    FakeRoutePlanner planner;
    expectError(planner.route({91, 0}, {0, 0}, TravelMode::Foot), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_EQ(planner.calls, 0);
    planner.result = RoutePlan{-1, 10, {{"depart"}}};
    expectError(planner.route({0, 0}, {0, 0}, TravelMode::Foot), ExternalServiceErrorKind::InvalidResponse);
    planner.result = RoutePlan{1, 10, {}};
    expectError(planner.route({0, 0}, {0, 0}, TravelMode::Foot), ExternalServiceErrorKind::InvalidResponse);
    FakeWeatherProvider weather;
    expectError(weather.forecast({0, 181}), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_EQ(weather.calls, 0);
    for (const auto& forecast :
         std::vector<WeatherForecast>{{.temperatureCelsius = std::numeric_limits<double>::quiet_NaN()},
                                      {.windSpeedKilometersPerHour = -1},
                                      {.maximumTemperatureCelsius = 5},
                                      {.maximumTemperatureCelsius = 1, .minimumTemperatureCelsius = 2},
                                      {.precipitationProbabilityPercent = 101}})
    {
        weather.result = forecast;
        expectError(weather.forecast({0, 0}), ExternalServiceErrorKind::InvalidResponse);
    }
}

TEST(NavigationServiceTest, FormatsRussianManeuversAndDoesNotInventRoundaboutExit)
{
    const RoutePlan plan{1500,
                         3660,
                         {{"depart", {}, "Первая", 100},
                          {"turn", "left", "Вторая", 20},
                          {"roundabout", {}, "Третья", 30, 3},
                          {"roundabout", {}, {}, 0},
                          {"arrive", "right"}}};
    const auto text = routeText(plan, TravelMode::Foot, "Начало", "Конец");
    EXPECT_NE(text.find("Поверните налево на Вторая и пройдите 20 м"), std::string::npos);
    EXPECT_NE(text.find("3-й съезд"), std::string::npos);
    EXPECT_NE(text.find("Продолжайте движение по круговому движению"), std::string::npos);
    EXPECT_NE(text.find("(справа)"), std::string::npos);
    EXPECT_NE(text.find("1.5 км"), std::string::npos);
    EXPECT_NE(text.find("1 ч 1 мин"), std::string::npos);
    EXPECT_NE(routeText(plan, TravelMode::Driving, "А", "Б").find("проедьте"), std::string::npos);
}

TEST(NavigationServiceTest, FormatsWeatherWithOptionalDailyDataAndUmbrellaThreshold)
{
    WeatherForecast forecast{.temperatureCelsius = 3.5,
                             .apparentTemperatureCelsius = -0.1,
                             .weatherCode = 61,
                             .maximumTemperatureCelsius = 5,
                             .minimumTemperatureCelsius = -2,
                             .precipitationProbabilityPercent = 29};
    auto text = weatherText(forecast);
    EXPECT_NE(text.find("Сейчас +4° (ощущается как 0°), дождь"), std::string::npos);
    EXPECT_NE(text.find("За день от -2° до +5°"), std::string::npos);
    EXPECT_EQ(text.find("зонт"), std::string::npos);
    forecast.precipitationProbabilityPercent = 30;
    EXPECT_NE(weatherText(forecast).find("зонт"), std::string::npos);
    forecast.weatherCode = 100;
    EXPECT_NE(weatherText(forecast).find("не определено"), std::string::npos);
    EXPECT_EQ(weatherText(WeatherForecast{}).find("За день"), std::string::npos);
}

} // namespace
} // namespace msa::core
