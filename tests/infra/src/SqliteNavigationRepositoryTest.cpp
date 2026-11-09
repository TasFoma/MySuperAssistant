#include "FakeClock.h"

#include "msa/core/CachedGeocoder.h"
#include "msa/core/PlaceReminderLocationService.h"
#include "msa/core/RouteHistoryService.h"
#include "msa/infra/SqliteGeocodingCacheRepository.h"
#include "msa/infra/SqliteReminderRepository.h"
#include "msa/infra/SqliteRouteHistoryRepository.h"
#include "msa/infra/SqliteSchema.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace msa::infra {
namespace {

using namespace std::chrono_literals;
constexpr auto kNow = std::chrono::local_seconds{std::chrono::seconds{1800000000}};

[[nodiscard]] core::RouteCalculation sample()
{
    return {.createdAt = kNow, .originName = " Начало\n🙂 ", .destinationName = "Конец '); --",
        .origin = {0, -180}, .destination = {90, 180}, .mode = core::TravelMode::Bike,
        .distanceMeters = 1234.5, .durationSeconds = 300.25, .approximate = true};
}

class StubGeocoder final : public core::Geocoder {
public:
    int calls{};
    std::vector<core::GeocodedPlace> places{{{0, 0}, "Адрес"}};

protected:
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::GeocodedPlace>> requestPlaces(const std::string& place) override;
};

core::ExternalServiceResult<std::vector<core::GeocodedPlace>> StubGeocoder::requestPlaces([[maybe_unused]] const std::string& place)
{
    ++calls;

    return places;
}

TEST(SqliteNavigationRepositoryTest, UpgradesVersionElevenAndPreservesExistingReminders)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("navigation.sqlite");
    std::int64_t identifier{};
    {
        auto migrations = sqliteMigrations();
        std::erase_if(migrations, [](const auto& migration) { return migration.version > 11; });
        SqliteDatabase database{path, migrations};
        ASSERT_EQ(database.schemaVersion(), 11);
        SqliteReminderRepository reminders{database};
        identifier = reminders.add(core::PlaceReminder{.title = "Хлеб", .place = "Магазин"});
    }
    SqliteDatabase database{path, sqliteMigrations()};
    EXPECT_EQ(database.schemaVersion(), 13);
    SqliteReminderRepository reminders{database};
    ASSERT_TRUE(reminders.find(identifier));
    EXPECT_EQ(std::get<core::PlaceReminder>(*reminders.find(identifier)).title, "Хлеб");
    SqliteRouteHistoryRepository history{database};
    EXPECT_TRUE(history.all().empty());
    SqliteGeocodingCacheRepository cache{database};
    EXPECT_FALSE(cache.find("endpoint", "Место"));
}

TEST(SqliteNavigationRepositoryTest, ReopensFullHistoryAndSeparatesApproximateStatistics)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("history.sqlite");
    std::int64_t first{};
    {
        SqliteDatabase database{path, sqliteMigrations()};
        SqliteRouteHistoryRepository repository{database};
        first = repository.add(sample());
        auto routed = sample();
        routed.approximate = false;
        routed.mode = core::TravelMode::Driving;
        EXPECT_GT(repository.add(routed), first);
        auto copy = repository.all();
        copy.front().originName = "Подмена";
        EXPECT_NE(repository.all().front().originName, "Подмена");
    }
    SqliteDatabase database{path, sqliteMigrations()};
    SqliteRouteHistoryRepository repository{database};
    core::FakeClock clock{kNow};
    core::RouteHistoryService history{repository, clock};
    const auto records = history.recent();
    ASSERT_EQ(records.size(), 2);
    EXPECT_EQ(records[1].identifier, first);
    EXPECT_EQ(records[1].originName, sample().originName);
    EXPECT_EQ(records[1].destinationName, sample().destinationName);
    EXPECT_EQ(records[1].createdAt, kNow);
    EXPECT_DOUBLE_EQ(records[1].origin.longitude, -180);
    EXPECT_DOUBLE_EQ(records[1].destination.latitude, 90);
    EXPECT_EQ(records[1].mode, core::TravelMode::Bike);
    EXPECT_DOUBLE_EQ(records[1].distanceMeters, 1234.5);
    EXPECT_DOUBLE_EQ(records[1].durationSeconds, 300.25);
    EXPECT_TRUE(records[1].approximate);
    const auto stats = history.statistics();
    EXPECT_EQ(stats.routedCount, 1);
    EXPECT_EQ(stats.approximateCount, 1);
    EXPECT_DOUBLE_EQ(stats.plannedDistanceMeters, 1234.5);
}

TEST(SqliteNavigationRepositoryTest, HistoryRejectsInvalidRecordsAndJoinsOuterRollback)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SqliteDatabase database{directory.filePath("history.sqlite"), sqliteMigrations()};
    SqliteRouteHistoryRepository repository{database};
    auto item = sample();
    item.identifier = 3;
    EXPECT_THROW(static_cast<void>(repository.add(item)), std::invalid_argument);
    item = sample();
    item.durationSeconds = std::numeric_limits<double>::infinity();
    EXPECT_THROW(static_cast<void>(repository.add(item)), std::invalid_argument);
    item = sample();
    item.mode = static_cast<core::TravelMode>(99);
    EXPECT_THROW(static_cast<void>(repository.add(item)), std::invalid_argument);
    EXPECT_THROW(database.transaction([&repository]() {
        EXPECT_GT(repository.add(sample()), 0);
        throw std::runtime_error{"Откат"};
    }), std::runtime_error);
    EXPECT_TRUE(repository.all().empty());
    database.execute("CREATE TRIGGER refuse_route BEFORE INSERT ON route_calculations BEGIN SELECT RAISE(ABORT, 'failure'); END");
    EXPECT_THROW(static_cast<void>(repository.add(sample())), std::runtime_error);
}

TEST(SqliteNavigationRepositoryTest, CacheReopensOrderedCandidatesAndEmptyEntriesWithIndependentKeys)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("cache.sqlite");
    const std::string query{"  Место\n🙂  "};
    {
        SqliteDatabase database{path, sqliteMigrations()};
        SqliteGeocodingCacheRepository cache{database};
        cache.save("first", query, {kNow, {{{0, 0}, " Первый\n "}, {{-90, 180}, "Второй"}}});
        cache.save("second", query, {kNow + 1h, {}});
    }
    SqliteDatabase database{path, sqliteMigrations()};
    SqliteGeocodingCacheRepository cache{database};
    auto entry = cache.find("first", query);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->storedAt, kNow);
    ASSERT_EQ(entry->places.size(), 2);
    EXPECT_EQ(entry->places[0].name, " Первый\n ");
    EXPECT_DOUBLE_EQ(entry->places[1].coordinates.latitude, -90);
    entry->places.clear();
    EXPECT_EQ(cache.find("first", query)->places.size(), 2);
    ASSERT_TRUE(cache.find("second", query));
    EXPECT_TRUE(cache.find("second", query)->places.empty());
    EXPECT_FALSE(cache.find("first", "Другой"));
}

TEST(SqliteNavigationRepositoryTest, CacheReplacementIsAtomicIncludingChildrenAndTimestamp)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SqliteDatabase database{directory.filePath("cache.sqlite"), sqliteMigrations()};
    SqliteGeocodingCacheRepository cache{database};
    cache.save("source", "place", {kNow, {{{0, 0}, "Исходный"}}});
    database.execute("CREATE TRIGGER refuse_candidate BEFORE INSERT ON geocoding_cache_places "
        "WHEN NEW.name = 'refused' BEGIN SELECT RAISE(ABORT, 'failure'); END");
    EXPECT_THROW(cache.save("source", "place", {kNow + 1h, {{{1, 1}, "Первый"}, {{2, 2}, "refused"}}}), std::runtime_error);
    auto entry = cache.find("source", "place");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->storedAt, kNow);
    ASSERT_EQ(entry->places.size(), 1);
    EXPECT_EQ(entry->places[0].name, "Исходный");
    EXPECT_THROW(database.transaction([&cache]() {
        cache.save("source", "place", {kNow + 1h, {}});
        throw std::runtime_error{"Откат"};
    }), std::runtime_error);
    EXPECT_EQ(cache.find("source", "place")->places.size(), 1);
    EXPECT_THROW(cache.save("source", "place", {kNow, {{{91, 0}, "Неверный"}}}), std::invalid_argument);
    EXPECT_THROW(cache.save(" ", "place", {kNow, {}}), std::invalid_argument);
}

TEST(SqliteNavigationRepositoryTest, CachedGeocoderReusesSqliteAfterReopenAndRefreshesExpiredResults)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("persistent.sqlite");
    StubGeocoder source;
    core::FakeClock clock{kNow};
    {
        SqliteDatabase database{path, sqliteMigrations()};
        SqliteGeocodingCacheRepository repository{database};
        core::CachedGeocoder geocoder{source, repository, clock, "endpoint|ru"};
        EXPECT_TRUE(std::holds_alternative<std::vector<core::GeocodedPlace>>(geocoder.search("Место")));
    }
    SqliteDatabase database{path, sqliteMigrations()};
    SqliteGeocodingCacheRepository repository{database};
    core::CachedGeocoder geocoder{source, repository, clock, "endpoint|ru"};
    EXPECT_TRUE(std::holds_alternative<std::vector<core::GeocodedPlace>>(geocoder.search("Место")));
    EXPECT_EQ(source.calls, 1);
    clock.advance(std::chrono::days{7});
    source.places.clear();
    EXPECT_TRUE(std::holds_alternative<core::ExternalServiceError>(geocoder.search("Место")));
    EXPECT_EQ(source.calls, 2);
    clock.advance(1h);
    EXPECT_TRUE(std::holds_alternative<core::ExternalServiceError>(geocoder.search("Место")));
    EXPECT_EQ(source.calls, 3);
}

TEST(SqliteNavigationRepositoryTest, ConfirmationCanRetryAfterSqlFailureAndPersistsFixedPoint)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("confirmation.sqlite");
    std::int64_t identifier{};
    {
        SqliteDatabase database{path, sqliteMigrations()};
        SqliteReminderRepository repository{database};
        identifier = repository.add(core::PlaceReminder{.title = "Хлеб", .place = "Магазин"});
        StubGeocoder geocoder;
        core::PlaceReminderLocationService service{repository, geocoder};
        const auto result = service.prepare(identifier, {0, 0});
        ASSERT_TRUE(std::holds_alternative<core::PlaceReminderLocationProposal>(result));
        const auto token = std::get<core::PlaceReminderLocationProposal>(result).identifier;
        database.execute("CREATE TRIGGER refuse_coordinates BEFORE UPDATE ON reminders BEGIN SELECT RAISE(ABORT, 'failure'); END");
        EXPECT_THROW(static_cast<void>(service.confirm(token, 0)), std::runtime_error);
        EXPECT_FALSE(std::get<core::PlaceReminder>(*repository.find(identifier)).coordinates);
        database.execute("DROP TRIGGER refuse_coordinates");
        EXPECT_TRUE(std::holds_alternative<core::GeoCoordinates>(service.confirm(token, 0)));
    }
    SqliteDatabase database{path, sqliteMigrations()};
    SqliteReminderRepository repository{database};
    const auto reminder = std::get<core::PlaceReminder>(*repository.find(identifier));
    ASSERT_TRUE(reminder.coordinates);
    EXPECT_DOUBLE_EQ(reminder.coordinates->latitude, 0);
    EXPECT_DOUBLE_EQ(reminder.coordinates->longitude, 0);
    EXPECT_EQ(reminder.place, "Магазин");
    EXPECT_EQ(reminder.status, core::ReminderStatus::Pending);
}

}  // namespace
}  // namespace msa::infra

