#include "FakeClock.h"
#include "InMemoryReminderRepository.h"

#include "msa/core/ReminderService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

TEST(PlaceReminderTest, CreatesWithOptionalAndBoundaryCoordinates)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto identifier = std::get<std::int64_t>(service.addPlace("Я", "М"));
    const auto saved = std::get<PlaceReminder>(*repository.find(identifier));
    EXPECT_EQ(saved.title, "Я");
    EXPECT_EQ(saved.place, "М");
    EXPECT_FALSE(saved.coordinates);
    EXPECT_EQ(saved.status, ReminderStatus::Pending);
    for (const auto coordinates : {GeoCoordinates{0, 0}, GeoCoordinates{-90, -180},
                                   GeoCoordinates{90, 180}}) {
        EXPECT_TRUE(std::holds_alternative<std::int64_t>(service.addPlace("Я", "М", coordinates)));
    }
    EXPECT_EQ(repository.all().size(), 4U);
}

TEST(PlaceReminderTest, RejectsInvalidInputWithoutWrites)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    EXPECT_TRUE(std::holds_alternative<ReminderRejection>(service.addPlace("", "М")));
    EXPECT_TRUE(std::holds_alternative<ReminderRejection>(service.addPlace("Я", "")));
    const auto identifier = std::get<std::int64_t>(service.addPlace("Я", "М", GeoCoordinates{}));
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();
    for (const auto coordinates : {GeoCoordinates{90.1, 0}, GeoCoordinates{-90.1, 0},
        GeoCoordinates{0, 180.1}, GeoCoordinates{0, -180.1}, GeoCoordinates{nan, 0},
        GeoCoordinates{0, nan}, GeoCoordinates{infinity, 0}, GeoCoordinates{0, -infinity}}) {
        const auto created = service.addPlace("Я", "М", coordinates);
        ASSERT_TRUE(std::holds_alternative<ReminderRejection>(created));
        EXPECT_FALSE(std::get<ReminderRejection>(created).reason.empty());
        const auto checked = service.checkNearPlace(coordinates);
        ASSERT_TRUE(std::holds_alternative<ReminderRejection>(checked));
        EXPECT_FALSE(std::get<ReminderRejection>(checked).reason.empty());
    }
    EXPECT_EQ(repository.all().size(), 1U);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(identifier)).status, ReminderStatus::Pending);
}

TEST(PlaceReminderTest, FiresOnlyNearbyPendingPlacesAndDoesNotRepeat)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto near = std::get<std::int64_t>(service.addPlace("Рядом", "Дом", GeoCoordinates{}));
    const auto far = std::get<std::int64_t>(service.addPlace("Далеко", "Дом", GeoCoordinates{1, 1}));
    const auto unknown = std::get<std::int64_t>(service.addPlace("Без координат", "Дом"));
    const auto fired = repository.add(PlaceReminder{.coordinates = GeoCoordinates{}, .status = ReminderStatus::Fired});
    const auto missed = repository.add(PlaceReminder{.coordinates = GeoCoordinates{}, .status = ReminderStatus::Missed});
    const auto timed = repository.add(TimedReminder{});
    const auto recurring = repository.add(RecurringReminder{});
    const auto due = std::get<std::vector<PlaceReminder>>(service.checkNearPlace({0, 0}));
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().identifier, near);
    EXPECT_EQ(due.front().status, ReminderStatus::Pending);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(near)).status, ReminderStatus::Fired);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(far)).status, ReminderStatus::Pending);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(unknown)).status, ReminderStatus::Pending);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(fired)).status, ReminderStatus::Fired);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(missed)).status, ReminderStatus::Missed);
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(timed)).status, ReminderStatus::Pending);
    EXPECT_EQ(std::get<RecurringReminder>(*repository.find(recurring)).dueAt, clock.now());
    EXPECT_TRUE(std::get<std::vector<PlaceReminder>>(service.checkNearPlace({0, 0})).empty());
}

TEST(PlaceReminderTest, DistinguishesInsideAndOutsideRadius)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    // На экваторе 0.001798 градуса — около 199.93 м, 0.001800 — около 200.15 м.
    const auto inside = std::get<std::int64_t>(service.addPlace("Внутри", "М", GeoCoordinates{0, 0.001798}));
    const auto outside = std::get<std::int64_t>(service.addPlace("Снаружи", "М", GeoCoordinates{0, 0.001800}));
    const auto due = std::get<std::vector<PlaceReminder>>(service.checkNearPlace({0, 0}));
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().identifier, inside);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(outside)).status, ReminderStatus::Pending);
}

TEST(PlaceReminderTest, HandlesDateLineAndPoles)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto across = std::get<std::int64_t>(service.addPlace("Через меридиан", "М", GeoCoordinates{0, -179.9995}));
    const auto pole = std::get<std::int64_t>(service.addPlace("Полюс", "М", GeoCoordinates{90, -180}));
    auto due = std::get<std::vector<PlaceReminder>>(service.checkNearPlace({0, 179.9995}));
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().identifier, across);
    due = std::get<std::vector<PlaceReminder>>(service.checkNearPlace({90, 180}));
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().identifier, pole);
}

TEST(PlaceReminderTest, CorruptStoredCoordinatesThrowWithoutChangingRecord)
{
    FakeClock clock{std::chrono::local_seconds{}};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto identifier = repository.add(PlaceReminder{.coordinates = GeoCoordinates{100, 0}});
    EXPECT_THROW(static_cast<void>(service.checkNearPlace({0, 0})), std::runtime_error);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(identifier)).status, ReminderStatus::Pending);
}

}  // namespace
}  // namespace msa::core

