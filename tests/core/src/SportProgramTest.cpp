#include "SportTestData.h"

#include <gtest/gtest.h>

using namespace msa::core;
TEST(SportProgramTest, RepeatedExerciseHasIndependentItems)
{
    auto session = msa::test::sportSession();
    auto second = session.items.front();
    second.identifier = "second";
    session.items.push_back(second);
    EXPECT_FALSE(validateSportSession(session));
    session.items.back().identifier = "item";
    EXPECT_TRUE(validateSportSession(session));
}
TEST(SportProgramTest, EnforcesRepetitionAndSetBoundaries)
{
    auto session = msa::test::sportSession();
    for (const auto repetitions : {1, 1000})
    {
        session.items.front().sets.front().repetitions = repetitions;
        EXPECT_FALSE(validateSportSession(session));
    }
    session.items.front().sets.front().repetitions = 1001;
    EXPECT_TRUE(validateSportSession(session));
    session = msa::test::sportSession();
    session.items.front().sets.clear();
    for (int number = 0; number < 100; ++number)
    {
        session.items.front().sets.push_back({"set-" + std::to_string(number), 1, BodyweightLoad{}, SportSide::Both});
    }
    EXPECT_FALSE(validateSportSession(session));
    session.items.front().sets.push_back({"extra", 1, BodyweightLoad{}, SportSide::Both});
    EXPECT_TRUE(validateSportSession(session));
}
TEST(SportProgramTest, IncompletePublishedContentIsRejected)
{
    auto exercise = msa::test::sportSession().items.front().exercise;
    exercise.published = false;
    exercise.technique.steps.clear();
    EXPECT_FALSE(validateSportExercise(exercise));
    exercise.published = true;
    EXPECT_TRUE(validateSportExercise(exercise));
}
TEST(SportProgramTest, DraftIsNotReadyProgram)
{
    EXPECT_TRUE(validateSportProgram({}));
    EXPECT_TRUE(validateSportSession({}));
}

TEST(SportProgramTest, UnpublishedMetadataCannotBecomeReadyTrainingPlan)
{
    auto session = msa::test::sportSession();
    session.items.front().exercise.published = false;
    EXPECT_TRUE(validateSportSession(session));
}
