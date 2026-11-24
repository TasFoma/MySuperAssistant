#include "msa/core/SportTimer.h"

#include <gtest/gtest.h>

using namespace msa::core;
using namespace std::chrono_literals;
TEST(SportTimerTest, PauseDoesNotAccumulateActiveTime)
{
    auto timer = startSportTimer(SportInstant{});
    timer = std::get<SportTimerState>(observeSportTimer(timer, {SportInstant{10s}, 10s}));
    timer = pauseSportTimer(timer);
    timer = pauseSportTimer(timer);
    timer = std::get<SportTimerState>(observeSportTimer(timer, {SportInstant{30s}, 20s}));
    timer = resumeSportTimer(timer);
    EXPECT_EQ(summarizeSportDuration(timer).active, 10s);
    EXPECT_EQ(summarizeSportDuration(timer).total, 30s);
}
TEST(SportTimerTest, RepeatedAbsenceAndChangedDecisionDoNotDoubleCount)
{
    auto timer = startSportTimer(SportInstant{});
    const SportAbsenceInterval interval{"absence", SportInstant{}, SportInstant{60s}};
    timer = std::get<SportTimerState>(registerSportAbsence(timer, interval));
    EXPECT_TRUE(summarizeSportDuration(timer).unresolved);
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Manual, 20s));
    timer = std::get<SportTimerState>(registerSportAbsence(timer, interval));
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Manual, 20s));
    EXPECT_EQ(summarizeSportDuration(timer).active, 20s);
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Activity));
    EXPECT_EQ(summarizeSportDuration(timer).active, 60s);
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Pause));
    EXPECT_EQ(summarizeSportDuration(timer).active, 0s);
    EXPECT_EQ(timer.begin, SportInstant{});
}
TEST(SportTimerTest, BackwardClockAllowsManualTimeWithoutInventedTotal)
{
    auto timer = startSportTimer(SportInstant{60s});
    timer = std::get<SportTimerState>(registerSportAbsence(timer, {"absence", SportInstant{60s}, SportInstant{10s}}));
    EXPECT_TRUE(
        std::holds_alternative<SportRejection>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Activity)));
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "absence", SportAbsenceDecision::Manual, 5s));
    const auto summary = summarizeSportDuration(timer);
    EXPECT_EQ(summary.active, 5s);
    EXPECT_FALSE(summary.total);
    EXPECT_TRUE(summary.warning);
}
TEST(SportTimerTest, RejectsOverlapsAndInvalidManualValues)
{
    auto timer = std::get<SportTimerState>(
        registerSportAbsence(startSportTimer(SportInstant{}), {"first", SportInstant{}, SportInstant{10s}}));
    EXPECT_TRUE(std::holds_alternative<SportRejection>(
        registerSportAbsence(timer, {"second", SportInstant{5s}, SportInstant{15s}})));
    EXPECT_TRUE(
        std::holds_alternative<SportRejection>(resolveSportAbsence(timer, "first", SportAbsenceDecision::Manual, 11s)));
    EXPECT_TRUE(std::holds_alternative<SportRejection>(observeSportTimer(timer, {SportInstant{11s}, -1s})));
    timer.manualDuration = 30s;
    EXPECT_EQ(summarizeSportDuration(timer).corrected, 30s);
    EXPECT_EQ(timer.observed, SportInstant{10s});
}

TEST(SportTimerTest, FrequentCheckpointsKeepContinuousPhaseCompactWithoutChangingSnapshots)
{
    auto timer = startSportTimer(SportInstant{});
    timer = std::get<SportTimerState>(observeSportTimer(timer, {SportInstant{1s}, 1s}));
    const auto first = timer;
    for (int index = 2; index <= 12000; ++index)
    {
        timer = std::get<SportTimerState>(observeSportTimer(timer, {SportInstant{std::chrono::seconds{index}}, 1s}));
    }
    EXPECT_EQ(timer.segments.size(), 1);
    EXPECT_EQ(summarizeSportDuration(timer).active, 12000s);
    EXPECT_EQ(first.segments.front().end, SportInstant{1s});
    timer = pauseSportTimer(timer);
    timer = std::get<SportTimerState>(observeSportTimer(timer, {SportInstant{12001s}, 1s}));
    EXPECT_EQ(timer.segments.size(), 2);
    EXPECT_EQ(summarizeSportDuration(timer).active, 12000s);
}

TEST(SportTimerTest, ClockDiscontinuityAllowsFurtherAbsencesWithManualResolution)
{
    auto timer = startSportTimer(SportInstant{});
    timer = std::get<SportTimerState>(registerSportAbsence(timer, {"first", SportInstant{}, SportInstant{10s}}));
    timer = std::get<SportTimerState>(registerSportAbsence(timer, {"backward", SportInstant{10s}, SportInstant{5s}}));
    timer = std::get<SportTimerState>(registerSportAbsence(timer, {"after", SportInstant{5s}, SportInstant{15s}}));
    EXPECT_TRUE(timer.clockWarning);
    EXPECT_FALSE(timer.absences.back().duration);
    timer = std::get<SportTimerState>(resolveSportAbsence(timer, "after", SportAbsenceDecision::Manual, 1s));
    EXPECT_EQ(timer.absences.back().active, 1s);
}
