#include "msa/core/Event.h"
#include "msa/core/Habit.h"
#include "msa/core/HabitLogEntry.h"
#include "msa/core/Reminder.h"
#include "msa/core/Task.h"
#include "msa/core/UserProfile.h"

#include <gtest/gtest.h>

#include <chrono>
#include <variant>

namespace msa::core {

TEST(PlannerTypesTest, EquatorAndPrimeMeridianAreResolvedPlaces)
{
    PlaceReminder unresolved;
    EXPECT_FALSE(unresolved.coordinates.has_value());

    const Reminder reminder = PlaceReminder{
        .title = "Проверить место",
        .place = "Пересечение экватора и нулевого меридиана",
        .coordinates = GeoCoordinates{0.0, 0.0},
    };
    const auto* place = std::get_if<PlaceReminder>(&reminder);
    ASSERT_NE(place, nullptr);
    ASSERT_TRUE(place->coordinates.has_value());
    EXPECT_DOUBLE_EQ(place->coordinates->latitude, 0.0);
    EXPECT_DOUBLE_EQ(place->coordinates->longitude, 0.0);
}

TEST(PlannerTypesTest, MidnightEventDiffersFromEventWithoutTime)
{
    const auto date = std::chrono::year{2026} / std::chrono::November / 5;
    const Event withoutTime{.title = "Выходной", .date = date};
    const Event atMidnight{
        .title = "Начало дежурства",
        .date = date,
        .time = std::chrono::minutes{0},
    };

    EXPECT_FALSE(withoutTime.time.has_value());
    ASSERT_TRUE(atMidnight.time.has_value());
    EXPECT_EQ(*atMidnight.time, std::chrono::minutes{0});
}

}  // namespace msa::core

