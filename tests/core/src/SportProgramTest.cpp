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

TEST(SportProgramTest, DumbbellQuantityMustMatchSpecifiedExerciseVariant)
{
    auto session = msa::test::sportSession();
    auto& item = session.items.front();
    item.exercise.loadKind = 1;
    item.exercise.requirements.dumbbellQuantity = 2;
    item.sets.front().load = DumbbellLoad{500, 1};
    EXPECT_TRUE(validateSportSession(session));
    item.sets.front().load = DumbbellLoad{500, 2};
    EXPECT_FALSE(validateSportSession(session));
    auto draft = std::get<SportWorkoutDraft>(SportWorkoutService{}.start(
        std::nullopt, session, "quantity", std::chrono::year{2026} / 11 / 24, SportInstant{}));
    auto actual = draft.items.front().actual.front();
    actual.state = SportSetState::Performed;
    actual.repetitions = 8;
    actual.load = DumbbellLoad{500, 1};
    EXPECT_TRUE(
        std::holds_alternative<SportRejection>(SportWorkoutService{}.recordSet(draft, item.identifier, actual)));
}
