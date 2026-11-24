#include "SportTestData.h"

#include <gtest/gtest.h>

using namespace msa::core;
TEST(SportWorkoutServiceTest, RejectsSecondActiveAndDoesNotMutatePlan)
{
    const auto active = msa::test::sportDraft();
    auto session = msa::test::sportSession();
    EXPECT_TRUE(std::holds_alternative<SportRejection>(
        SportWorkoutService{}.start(active, session, "second", active.date, {})));
    session.items.front().sets.front().repetitions = 99;
    EXPECT_EQ(active.originalPlan.items.front().sets.front().repetitions, 8);
    EXPECT_EQ(active.items.front().actual.front().state, SportSetState::Pending);
    EXPECT_FALSE(active.items.front().actual.front().repetitions);
}
TEST(SportWorkoutServiceTest, AllSkippedCanFinishButIsNotPerformed)
{
    const auto draft = msa::test::sportDraft();
    EXPECT_TRUE(std::holds_alternative<SportRejection>(SportWorkoutService{}.finish(draft, "revision")));
    const auto skipped = std::get<SportWorkoutDraft>(SportWorkoutService{}.skipExercise(draft, "item"));
    const auto result = std::get<SportWorkoutRevision>(SportWorkoutService{}.finish(skipped, "revision"));
    EXPECT_TRUE(result.entirelySkipped);
    EXPECT_FALSE(result.workout.items.front().actual.front().repetitions);
}
TEST(SportWorkoutServiceTest, ReplacementKeepsAlreadyPerformedExercise)
{
    const auto performed = msa::test::sportResult().workout;
    auto replacement = msa::test::sportSession().items.front();
    replacement.identifier = "replacement";
    replacement.exercise.identifier = "other";
    const auto changed =
        std::get<SportWorkoutDraft>(SportWorkoutService{}.replaceExercise(performed, "item", replacement));
    ASSERT_EQ(changed.items.size(), 2U);
    EXPECT_EQ(changed.items.front().actual.front().state, SportSetState::Performed);
    EXPECT_EQ(changed.items.back().replacesItem, "item");
    EXPECT_EQ(changed.items.back().actual.front().state, SportSetState::Pending);
}
TEST(SportWorkoutServiceTest, CorrectionUsesSameIdentityAndKeepsOriginalTimes)
{
    const auto current = msa::test::sportResult();
    auto edited = current.workout;
    edited.items.front().actual.front().repetitions = 12;
    edited.timer.begin += std::chrono::hours{1};
    edited.timer.manualDuration = std::chrono::minutes{10};
    const auto corrected =
        std::get<SportWorkoutRevision>(SportWorkoutService{}.correct(current, current.identifier, edited, "next"));
    EXPECT_EQ(corrected.workout.identifier, current.workout.identifier);
    EXPECT_EQ(corrected.parentRevision, current.identifier);
    EXPECT_EQ(corrected.workout.timer.begin, current.workout.timer.begin);
    EXPECT_EQ(current.workout.items.front().actual.front().repetitions, 8);
    EXPECT_TRUE(
        std::holds_alternative<SportRejection>(SportWorkoutService{}.correct(current, "stale", edited, "next")));
}
TEST(SportWorkoutServiceTest, MidnightDoesNotChangeExplicitWorkoutDate)
{
    auto draft = msa::test::sportResult().workout;
    draft.timer.observed += std::chrono::hours{25};
    const auto result = std::get<SportWorkoutRevision>(SportWorkoutService{}.finish(draft, "late"));
    EXPECT_EQ(result.workout.date, std::chrono::year{2026} / 11 / 24);
}

TEST(SportWorkoutServiceTest, CorrectionPreservesStartMetadataAndRestrictionSnapshot)
{
    auto current = msa::test::sportResult();
    current.workout.programIdentifier = "original-program";
    current.workout.programRevision = "original-program-revision";
    current.workout.utcOffsetMinutes = 180;
    current.workout.restrictionsSnapshot = "Исходные ограничения";
    auto edited = current.workout;
    edited.programIdentifier = "other-program";
    edited.programRevision = "other-revision";
    edited.utcOffsetMinutes = 0;
    edited.restrictionsSnapshot = "Поздние ограничения";
    const auto result = SportWorkoutService{}.correct(current, current.identifier, edited, "corrected-metadata");
    ASSERT_TRUE(std::holds_alternative<SportWorkoutRevision>(result));
    const auto& corrected = std::get<SportWorkoutRevision>(result).workout;
    EXPECT_EQ(corrected.programIdentifier, current.workout.programIdentifier);
    EXPECT_EQ(corrected.programRevision, current.workout.programRevision);
    EXPECT_EQ(corrected.utcOffsetMinutes, 180);
    EXPECT_EQ(corrected.restrictionsSnapshot, "Исходные ограничения");
}
