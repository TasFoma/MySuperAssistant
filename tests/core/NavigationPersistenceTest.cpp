#include "FakeClock.h"
#include "InMemoryGeocodingCacheRepository.h"
#include "InMemoryReminderRepository.h"
#include "InMemoryRouteHistoryRepository.h"
#include "msa/core/CachedGeocoder.h"
#include "msa/core/NavigationService.h"
#include "msa/core/PlaceReminderLocationService.h"
#include "msa/core/RouteHistoryService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <limits>
#include <stdexcept>

namespace msa::core
{
namespace
{

using namespace std::chrono_literals;
constexpr auto kNow = std::chrono::local_seconds{std::chrono::seconds{1800000000}};

class StubGeocoder final : public Geocoder
{
public:
    ExternalServiceResult<std::vector<GeocodedPlace>> answer{
        std::vector<GeocodedPlace>{{{0, 1}, "Дальше"}, {{0, 0}, "Ближе"}}};
    int calls{};
    std::function<void()> onSearch{};

protected:
    [[nodiscard]] ExternalServiceResult<std::vector<GeocodedPlace>> requestPlaces(const std::string& place) override;
};

ExternalServiceResult<std::vector<GeocodedPlace>> StubGeocoder::requestPlaces([[maybe_unused]] const std::string& place)
{
    ++calls;
    if (onSearch)
    {
        onSearch();
    }

    return answer;
}

class StubPlanner final : public RoutePlanner
{
public:
    ExternalServiceResult<RoutePlan> answer{RoutePlan{100, 60, {{"depart"}, {"arrive"}}}};

protected:
    [[nodiscard]] ExternalServiceResult<RoutePlan> requestRoute(GeoCoordinates origin, GeoCoordinates destination,
                                                                TravelMode mode) override;
};

ExternalServiceResult<RoutePlan> StubPlanner::requestRoute([[maybe_unused]] GeoCoordinates origin,
                                                           [[maybe_unused]] GeoCoordinates destination,
                                                           [[maybe_unused]] TravelMode mode)
{
    return answer;
}

template <typename ValueType>
void expectError(const ExternalServiceResult<ValueType>& result, ExternalServiceErrorKind kind)
{
    ASSERT_TRUE(std::holds_alternative<ExternalServiceError>(result));
    EXPECT_EQ(std::get<ExternalServiceError>(result).kind, kind);
}

[[nodiscard]] RouteCalculation calculation()
{
    return {.originName = "  Начало\n ",
            .destinationName = "Конец",
            .origin = {0, 0},
            .destination = {0, 1},
            .mode = TravelMode::Bike,
            .distanceMeters = 100,
            .durationSeconds = 60};
}

TEST(NavigationPersistenceTest, PositiveCacheExpiresAtSevenDaysAndReturnsIndependentCandidates)
{
    StubGeocoder source;
    InMemoryGeocodingCacheRepository repository;
    FakeClock clock{kNow};
    CachedGeocoder geocoder{source, repository, clock, "endpoint|ru"};
    auto result = geocoder.search("  Магазин  ");
    ASSERT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(result));
    std::get<std::vector<GeocodedPlace>>(result)[0].name = "Подмена";
    clock.advance(std::chrono::days{7} - 1s);
    result = geocoder.search("Магазин");
    EXPECT_EQ(source.calls, 1);
    EXPECT_EQ(std::get<std::vector<GeocodedPlace>>(result)[0].name, "Дальше");
    clock.advance(1s);
    EXPECT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(geocoder.search("Магазин")));
    EXPECT_EQ(source.calls, 2);
    CachedGeocoder reopened{source, repository, clock, "endpoint|ru"};
    EXPECT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(reopened.search("Магазин")));
    EXPECT_EQ(source.calls, 2);
}

TEST(NavigationPersistenceTest, NegativeCacheExpiresAtOneHourAndNeverCachesTransportErrors)
{
    StubGeocoder source;
    source.answer = std::vector<GeocodedPlace>{};
    InMemoryGeocodingCacheRepository repository;
    FakeClock clock{kNow};
    CachedGeocoder geocoder{source, repository, clock, "endpoint"};
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::NotFound);
    clock.advance(1h - 1s);
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::NotFound);
    EXPECT_EQ(source.calls, 1);
    clock.advance(1s);
    source.answer = ExternalServiceError{ExternalServiceErrorKind::Unavailable, "Недоступен"};
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::Unavailable);
    expectError(geocoder.search("Место"), ExternalServiceErrorKind::Unavailable);
    EXPECT_EQ(source.calls, 3);
    EXPECT_EQ(repository.find("endpoint", "Место")->storedAt, kNow);
}

TEST(NavigationPersistenceTest, CacheSeparatesProvidersInvalidatesFutureEntriesAndDatesAfterSearch)
{
    StubGeocoder source;
    InMemoryGeocodingCacheRepository repository;
    FakeClock clock{kNow};
    CachedGeocoder first{source, repository, clock, "first"};
    CachedGeocoder second{source, repository, clock, "second"};
    ASSERT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(first.search("Место")));
    ASSERT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(second.search("Место")));
    EXPECT_EQ(source.calls, 2);
    clock.set(kNow - 1h);
    source.onSearch = [&clock]() { clock.advance(10s); };
    ASSERT_TRUE(std::holds_alternative<std::vector<GeocodedPlace>>(first.search("Место")));
    EXPECT_EQ(source.calls, 3);
    EXPECT_EQ(repository.find("first", "Место")->storedAt, clock.now());
    repository.failSave = true;
    EXPECT_THROW(static_cast<void>(first.search("Другой запрос")), std::runtime_error);
    EXPECT_THROW((CachedGeocoder{source, repository, clock, " "}), std::invalid_argument);
}

TEST(NavigationPersistenceTest, HistoryKeepsAllCalculationsAndLatestFiveByIdentifierWithSeparateEstimates)
{
    InMemoryRouteHistoryRepository repository;
    FakeClock clock{kNow};
    RouteHistoryService history{repository, clock};
    for (int index = 0; index < 7; ++index)
    {
        auto item = calculation();
        item.approximate = index % 2 != 0;
        EXPECT_GT(history.record(item), 0);
        clock.advance(-1h);
    }
    EXPECT_EQ(repository.all().size(), 7);
    auto recent = history.recent();
    ASSERT_EQ(recent.size(), 5);
    EXPECT_EQ(recent.front().identifier, 7);
    EXPECT_EQ(recent.back().identifier, 3);
    EXPECT_EQ(recent.front().createdAt, kNow - 6h);
    recent.front().originName = "Подмена";
    EXPECT_EQ(history.recent().front().originName, calculation().originName);
    const auto statistics = history.statistics();
    EXPECT_EQ(statistics.routedCount, 4);
    EXPECT_EQ(statistics.approximateCount, 3);
    EXPECT_DOUBLE_EQ(statistics.plannedDistanceMeters, 400);
    EXPECT_DOUBLE_EQ(statistics.plannedDurationSeconds, 240);
}

TEST(NavigationPersistenceTest, NavigationRecordsSuccessfulAndApproximateRoutesButNotFailures)
{
    StubGeocoder geocoder;
    StubPlanner planner;
    InMemoryRouteHistoryRepository repository;
    FakeClock clock{kNow};
    RouteHistoryService history{repository, clock};
    NavigationService navigation{geocoder, planner, history};
    ASSERT_TRUE(
        std::holds_alternative<RoutePlan>(navigation.routeBetween("Начало", "Конец", {0, 0}, TravelMode::Foot)));
    planner.answer = ExternalServiceError{ExternalServiceErrorKind::Unavailable, "Недоступен"};
    ASSERT_TRUE(std::holds_alternative<RoutePlan>(navigation.routeFrom({0, 0}, "Конец", TravelMode::Driving)));
    planner.answer = ExternalServiceError{ExternalServiceErrorKind::NotFound, "Нет маршрута"};
    expectError(navigation.routeFrom({0, 0}, "Конец", TravelMode::Foot), ExternalServiceErrorKind::NotFound);
    ASSERT_EQ(repository.all().size(), 2);
    const auto records = history.recent();
    EXPECT_TRUE(records[0].approximate);
    EXPECT_EQ(records[0].originName, "Моё местоположение");
    EXPECT_EQ(records[1].originName, "Начало");
    EXPECT_EQ(records[1].destinationName, "Конец");
    EXPECT_DOUBLE_EQ(records[1].destination.longitude, 0);
    planner.answer = RoutePlan{100, 60, {{"depart"}}};
    repository.failAdd = true;
    EXPECT_THROW(static_cast<void>(navigation.routeFrom({0, 0}, "Конец", TravelMode::Foot)), std::runtime_error);
    EXPECT_EQ(repository.all().size(), 2);
}

TEST(NavigationPersistenceTest, HistoryRejectsInvalidCalculationsAndDetectsStatisticOverflow)
{
    InMemoryRouteHistoryRepository repository;
    FakeClock clock{kNow};
    RouteHistoryService history{repository, clock};
    auto item = calculation();
    item.distanceMeters = -1;
    EXPECT_THROW(static_cast<void>(history.record(item)), std::invalid_argument);
    EXPECT_TRUE(repository.all().empty());
    item = calculation();
    item.distanceMeters = std::numeric_limits<double>::max();
    EXPECT_GT(history.record(item), 0);
    EXPECT_GT(history.record(item), 0);
    EXPECT_THROW(static_cast<void>(history.statistics()), std::overflow_error);
}

TEST(NavigationPersistenceTest, ProposalRequiresExplicitChoiceAndPinsPointWithoutMutatingOtherFields)
{
    InMemoryReminderRepository repository;
    const auto identifier = repository.add(PlaceReminder{.title = "Купить хлеб", .place = "Магазин"});
    StubGeocoder geocoder;
    PlaceReminderLocationService service{repository, geocoder};
    auto result = service.prepare(identifier, {0, 0});
    ASSERT_TRUE(std::holds_alternative<PlaceReminderLocationProposal>(result));
    auto proposal = std::get<PlaceReminderLocationProposal>(result);
    EXPECT_EQ(proposal.candidates.front().name, "Ближе");
    EXPECT_FALSE(std::get<PlaceReminder>(*repository.find(identifier)).coordinates);
    proposal.candidates[0].coordinates = {90, 90};
    const auto confirmed = service.confirm(proposal.identifier, 0);
    ASSERT_TRUE(std::holds_alternative<GeoCoordinates>(confirmed));
    const auto stored = std::get<PlaceReminder>(*repository.find(identifier));
    ASSERT_TRUE(stored.coordinates);
    EXPECT_DOUBLE_EQ(stored.coordinates->latitude, 0);
    EXPECT_DOUBLE_EQ(stored.coordinates->longitude, 0);
    EXPECT_EQ(stored.title, "Купить хлеб");
    EXPECT_EQ(stored.place, "Магазин");
    EXPECT_EQ(stored.status, ReminderStatus::Pending);
    expectError(service.confirm(proposal.identifier, 0), ExternalServiceErrorKind::InvalidRequest);
    expectError(service.prepare(identifier, {10, 10}), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_EQ(geocoder.calls, 1);
}

TEST(NavigationPersistenceTest, ProposalDetectsReminderEditDuringSearchAndBeforeConfirmation)
{
    InMemoryReminderRepository repository;
    const auto identifier = repository.add(PlaceReminder{.title = "Хлеб", .place = "Магазин"});
    StubGeocoder geocoder;
    PlaceReminderLocationService service{repository, geocoder};
    geocoder.onSearch = [&repository, identifier]()
    {
        auto reminder = std::get<PlaceReminder>(*repository.find(identifier));
        reminder.place = "Другой адрес";
        EXPECT_TRUE(repository.update(reminder));
    };
    expectError(service.prepare(identifier, {0, 0}), ExternalServiceErrorKind::InvalidRequest);
    geocoder.onSearch = {};
    const auto result = service.prepare(identifier, {0, 0});
    ASSERT_TRUE(std::holds_alternative<PlaceReminderLocationProposal>(result));
    const auto token = std::get<PlaceReminderLocationProposal>(result).identifier;
    auto reminder = std::get<PlaceReminder>(*repository.find(identifier));
    reminder.title = "Новое название";
    EXPECT_TRUE(repository.update(reminder));
    expectError(service.confirm(token, 0), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_FALSE(std::get<PlaceReminder>(*repository.find(identifier)).coordinates);
}

TEST(NavigationPersistenceTest, CancelsSupersedesAndRejectsInvalidProposalChoicesWithoutWriting)
{
    InMemoryReminderRepository repository;
    const auto identifier = repository.add(PlaceReminder{.title = "Хлеб", .place = "Магазин"});
    StubGeocoder geocoder;
    PlaceReminderLocationService service{repository, geocoder};
    const auto first = service.prepare(identifier, {0, 0});
    const auto firstToken = std::get<PlaceReminderLocationProposal>(first).identifier;
    const auto second = service.prepare(identifier, {0, 0});
    const auto secondToken = std::get<PlaceReminderLocationProposal>(second).identifier;
    expectError(service.confirm(firstToken, 0), ExternalServiceErrorKind::InvalidRequest);
    expectError(service.confirm(secondToken, 99), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(service.cancel(secondToken));
    EXPECT_FALSE(service.cancel(secondToken));
    expectError(service.confirm(secondToken, 0), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_FALSE(std::get<PlaceReminder>(*repository.find(identifier)).coordinates);
    expectError(service.prepare(identifier, {91, 0}), ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(repository.remove(identifier));
    expectError(service.prepare(identifier, {0, 0}), ExternalServiceErrorKind::InvalidRequest);
}

TEST(NavigationPersistenceTest, RefusesReentryAndCanPrepareAfterGeocoderThrows)
{
    InMemoryReminderRepository repository;
    const auto identifier = repository.add(PlaceReminder{.title = "Хлеб", .place = "Магазин"});
    StubGeocoder geocoder;
    PlaceReminderLocationService service{repository, geocoder};
    geocoder.onSearch = [&service, identifier]()
    {
        expectError(service.prepare(identifier, {0, 0}), ExternalServiceErrorKind::Unavailable);
        throw std::runtime_error{"Отказ поиска"};
    };
    EXPECT_THROW(static_cast<void>(service.prepare(identifier, {0, 0})), std::runtime_error);
    geocoder.onSearch = {};
    EXPECT_TRUE(std::holds_alternative<PlaceReminderLocationProposal>(service.prepare(identifier, {0, 0})));
}

} // namespace
} // namespace msa::core
