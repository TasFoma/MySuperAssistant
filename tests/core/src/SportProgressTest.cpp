#include "SportTestData.h"
#include "msa/core/SportProgress.h"

#include <gtest/gtest.h>

using namespace msa::core;
TEST(SportProgressTest, UsesCurrentCorrectionAndRejectsDuplicateWorkout)
{
    const auto current = msa::test::sportResult();
    auto edited = current.workout;
    edited.items.front().actual.front().repetitions = 12;
    const auto next =
        std::get<SportWorkoutRevision>(SportWorkoutService{}.correct(current, "revision", edited, "next"));
    const std::vector<SportWorkoutRecord> records{{next}};
    const auto series = std::get<std::vector<SportProgressSeries>>(buildSportProgress(records));
    ASSERT_EQ(series.size(), 1U);
    EXPECT_EQ(series.front().results.front().actual.repetitions, 12);
    const std::vector<SportWorkoutRecord> duplicate{{current}, {next}};
    EXPECT_TRUE(std::holds_alternative<SportRejection>(buildSportProgress(duplicate)));
}
TEST(SportProgressTest, SeparatesConditionsAndSides)
{
    const auto first = msa::test::sportResult();
    auto second = first;
    second.workout.identifier = "second";
    second.workout.items.front().plan.conditions.supportHeightMillimeters = 500;
    const std::vector<SportWorkoutRecord> records{{first}, {second}};
    EXPECT_EQ(std::get<std::vector<SportProgressSeries>>(buildSportProgress(records)).size(), 2U);
}
TEST(SportProgressTest, EntirelySkippedAndMissingDataNeverCreateZeroResults)
{
    auto result = msa::test::sportResult();
    result.entirelySkipped = true;
    const std::vector<SportWorkoutRecord> records{{result}};
    EXPECT_TRUE(std::get<std::vector<SportProgressSeries>>(buildSportProgress(records)).empty());
    EXPECT_TRUE(std::get<std::vector<SportProgressSeries>>(buildSportProgress({})).empty());
}
