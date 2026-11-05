#include "FakeClock.h"
#include "InMemoryHabitRepository.h"
#include "msa/core/HabitService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <variant>

namespace msa::core {
namespace {

using std::chrono::year;

TEST(HabitServiceTest, RejectsInvalidNumbersWithoutSaving)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryHabitRepository repository;
    HabitService service{clock, repository};
    for (const auto value : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        const auto result = service.addHabit("Чтение", value, "стр", HabitPeriod::Day);
        ASSERT_TRUE(std::holds_alternative<HabitRejection>(result));
        EXPECT_FALSE(std::get<HabitRejection>(result).reason.empty());
    }
    EXPECT_TRUE(repository.all().empty());
    const auto identifier = std::get<std::int64_t>(service.addHabit("Чтение", 20.0, "стр", HabitPeriod::Day));
    for (const auto value : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_TRUE(std::holds_alternative<HabitRejection>(service.logHabit(identifier, value)));
    }
    EXPECT_TRUE(repository.logs(identifier).empty());
    EXPECT_TRUE(std::holds_alternative<HabitRejection>(service.logHabit(identifier + 1, 1.0)));
}

TEST(HabitServiceTest, DailyProgressSumsFractionsExceedsTargetAndResetsAtMidnight)
{
    const auto date = year{2026} / 12 / 31;
    FakeClock clock{std::chrono::local_days{date} + std::chrono::hours{23}};
    InMemoryHabitRepository repository;
    HabitService service{clock, repository};
    const auto identifier = std::get<std::int64_t>(service.addHabit("Чтение", 20.0, "стр", HabitPeriod::Day));
    EXPECT_EQ(service.habits().front().progress, 0.0);
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(service.logHabit(identifier, 10.5)));
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(service.logHabit(identifier, 15.0)));
    EXPECT_DOUBLE_EQ(service.habits().front().progress, 25.5);
    EXPECT_EQ(repository.logs(identifier).front().date, date);
    clock.advance(std::chrono::hours{1});
    EXPECT_DOUBLE_EQ(service.habits().front().progress, 0.0);
    EXPECT_EQ(repository.logs(identifier).size(), 2U);
}

TEST(HabitServiceTest, WeeklyProgressIncludesMondayAndTodayButExcludesFutureAndPreviousWeek)
{
    FakeClock clock{std::chrono::local_days{year{2027} / 1 / 3}};
    InMemoryHabitRepository repository;
    HabitService service{clock, repository};
    const auto identifier = std::get<std::int64_t>(service.addHabit("Чтение", 20.0, "стр", HabitPeriod::Week));
    for (const auto date : {year{2026} / 12 / 27, year{2026} / 12 / 28,
                            year{2027} / 1 / 3, year{2027} / 1 / 4,
                            year{2027} / 1 / 20}) {
        ASSERT_TRUE(repository.addLog(HabitLogEntry{.habitIdentifier = identifier, .date = date, .value = 1.0}));
    }
    EXPECT_DOUBLE_EQ(service.habits().front().progress, 2.0);
    clock.advance(std::chrono::days{1});
    EXPECT_DOUBLE_EQ(service.habits().front().progress, 1.0);
}

TEST(HabitServiceTest, DailyProgressExcludesOtherDatesAndKeepsHabitsSeparate)
{
    const auto date = year{2026} / 11 / 5;
    FakeClock clock{std::chrono::local_days{date}};
    InMemoryHabitRepository repository;
    HabitService service{clock, repository};
    EXPECT_TRUE(service.habits().empty());
    const auto first = std::get<std::int64_t>(service.addHabit("Чтение", 20.0, "стр", HabitPeriod::Day));
    const auto second = std::get<std::int64_t>(service.addHabit("Ходьба", 5.0, "км", HabitPeriod::Day));
    static_cast<void>(repository.addLog(HabitLogEntry{.habitIdentifier = first, .date = year{2026} / 11 / 4, .value = 9.0}));
    static_cast<void>(repository.addLog(HabitLogEntry{.habitIdentifier = first, .date = year{2026} / 11 / 6, .value = 9.0}));
    static_cast<void>(service.logHabit(first, 3.0));
    static_cast<void>(service.logHabit(second, 2.0));
    auto progress = service.habits();
    ASSERT_EQ(progress.size(), 2U);
    EXPECT_EQ(progress[0].habit.identifier, first);
    EXPECT_EQ(progress[1].habit.identifier, second);
    EXPECT_DOUBLE_EQ(progress[0].progress, 3.0);
    EXPECT_DOUBLE_EQ(progress[1].progress, 2.0);
    progress[0].habit.name = "Изменено";
    EXPECT_EQ(repository.find(first)->name, "Чтение");
}

TEST(HabitServiceTest, DetectsOverflowOfStoredProgress)
{
    const auto date = year{2026} / 11 / 5;
    FakeClock clock{std::chrono::local_days{date}};
    InMemoryHabitRepository repository;
    HabitService service{clock, repository};
    const auto identifier = std::get<std::int64_t>(service.addHabit("Чтение", 20.0, "стр", HabitPeriod::Day));
    static_cast<void>(service.logHabit(identifier, std::numeric_limits<double>::max()));
    static_cast<void>(service.logHabit(identifier, std::numeric_limits<double>::max()));
    EXPECT_THROW(static_cast<void>(service.habits()), std::runtime_error);
}

}  // namespace
}  // namespace msa::core
