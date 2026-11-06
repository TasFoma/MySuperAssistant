#include "../core/FakeClock.h"

#include "msa/core/HabitService.h"
#include "msa/infra/SqliteHabitRepository.h"
#include "msa/infra/SqliteReminderRepository.h"
#include "msa/infra/SqliteSchema.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

namespace msa::infra {
namespace {

using std::chrono::year;
constexpr auto kDate = year{2026} / 11 / 6;
constexpr std::int64_t kMissingIdentifier = 999999;

[[nodiscard]] core::Habit sampleHabit()
{
    return {.name = "  Чтение\n🙂 '); DELETE FROM habits; --  ", .target = 2.5,
        .unit = "  стр\n🙂  ", .period = core::HabitPeriod::Week};
}

void expectHabit(const core::Habit& actual, const core::Habit& expected)
{
    EXPECT_EQ(actual.identifier, expected.identifier);
    EXPECT_EQ(actual.name, expected.name);
    EXPECT_DOUBLE_EQ(actual.target, expected.target);
    EXPECT_EQ(actual.unit, expected.unit);
    EXPECT_EQ(actual.period, expected.period);
}

class SqliteHabitRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-habit-test-XXXXXX")};
};

void SqliteHabitRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteHabitRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("привычки.sqlite"));
}

TEST_F(SqliteHabitRepositoryTest, UpgradesVersionTenAndReopensWithFullHabitAndRepeatedLogs)
{
    auto migrations = sqliteMigrations();
    migrations.resize(9);
    {
        SqliteDatabase database{path(), migrations};
        SqliteReminderRepository reminders{database};
        ASSERT_GT(reminders.add(core::TimedReminder{.title = "Прежнее"}), 0);
    }
    auto expected = sampleHabit();
    expected.name.append(20000, 'x');
    expected.name.append("\0конец", sizeof("\0конец") - 1);
    expected.unit.append("\0ед", sizeof("\0ед") - 1);
    std::int64_t lastLogIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        EXPECT_EQ(database.schemaVersion(), 11);
        SqliteReminderRepository reminders{database};
        ASSERT_EQ(reminders.all().size(), 1U);
        SqliteHabitRepository repository{database};
        EXPECT_TRUE(repository.all().empty());
        const auto original = expected;
        expected.identifier = repository.add(original);
        EXPECT_EQ(original.identifier, 0);
        ASSERT_GT(expected.identifier, 0);
        const core::HabitLogEntry entry{.habitIdentifier = expected.identifier, .date = kDate, .value = 0.25};
        const auto first = repository.addLog(entry);
        const auto second = repository.addLog(entry);
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_GT(*second, *first);
        EXPECT_EQ(entry.identifier, 0);
        lastLogIdentifier = *second;
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    const auto found = repository.find(expected.identifier);
    ASSERT_TRUE(found);
    expectHabit(*found, expected);
    auto entries = repository.logs(expected.identifier);
    ASSERT_EQ(entries.size(), 2U);
    for (const auto& entry : entries) {
        EXPECT_EQ(entry.habitIdentifier, expected.identifier);
        EXPECT_EQ(entry.date, kDate);
        EXPECT_DOUBLE_EQ(entry.value, 0.25);
    }
    entries.front().value = 100.0;
    EXPECT_DOUBLE_EQ(repository.logs(expected.identifier).front().value, 0.25);
    auto copies = repository.all();
    copies.front().name.clear();
    expectHabit(*repository.find(expected.identifier), expected);
    const auto nextLog = repository.addLog({.habitIdentifier = expected.identifier,
        .date = std::chrono::year::min() / 1 / 1, .value = 1.5});
    ASSERT_TRUE(nextLog);
    EXPECT_GT(*nextLog, lastLogIdentifier);
    EXPECT_GT(repository.add(sampleHabit()), expected.identifier);
}

TEST_F(SqliteHabitRepositoryTest, UpdateKeepsLogAndDeleteCascadesOnlySelectedHabit)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    auto habit = sampleHabit();
    habit.identifier = repository.add(habit);
    const auto otherIdentifier = repository.add(sampleHabit());
    ASSERT_TRUE(repository.addLog({.habitIdentifier = habit.identifier, .date = kDate, .value = 1.0}));
    ASSERT_TRUE(repository.addLog({.habitIdentifier = otherIdentifier, .date = kDate, .value = 2.0}));
    habit.name = "Изменено";
    habit.target = 12.75;
    habit.unit = "мин";
    habit.period = core::HabitPeriod::Day;
    ASSERT_TRUE(repository.update(habit));
    expectHabit(*repository.find(habit.identifier), habit);
    EXPECT_EQ(repository.logs(habit.identifier).size(), 1U);
    EXPECT_FALSE(repository.find(kMissingIdentifier));
    auto missing = habit;
    missing.identifier = kMissingIdentifier;
    EXPECT_FALSE(repository.update(missing));
    EXPECT_FALSE(repository.remove(kMissingIdentifier));
    EXPECT_FALSE(repository.addLog({.habitIdentifier = kMissingIdentifier, .date = kDate, .value = 1.0}));
    EXPECT_TRUE(repository.logs(kMissingIdentifier).empty());
    ASSERT_TRUE(repository.remove(habit.identifier));
    EXPECT_FALSE(repository.remove(habit.identifier));
    EXPECT_TRUE(repository.logs(habit.identifier).empty());
    EXPECT_EQ(repository.logs(otherIdentifier).size(), 1U);
    EXPECT_GT(repository.add(sampleHabit()), otherIdentifier);
}

TEST_F(SqliteHabitRepositoryTest, RejectsInvalidNumbersIdsDatesAndPeriodsWithoutWrites)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    auto habit = sampleHabit();
    habit.identifier = repository.add(habit);
    EXPECT_THROW(static_cast<void>(repository.add(habit)), std::invalid_argument);
    for (const auto value : std::array{0.0, -1.0, std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid = habit;
        invalid.target = value;
        EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
        EXPECT_THROW(static_cast<void>(repository.addLog(
            {.habitIdentifier = habit.identifier, .date = kDate, .value = value})), std::invalid_argument);
    }
    auto invalid = habit;
    invalid.period = static_cast<core::HabitPeriod>(255);
    EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.addLog(
        {.identifier = 1, .habitIdentifier = habit.identifier, .date = kDate, .value = 1.0})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.addLog(
        {.habitIdentifier = habit.identifier, .date = year{2026} / 2 / 29, .value = 1.0})), std::invalid_argument);
    EXPECT_TRUE(repository.logs(habit.identifier).empty());
    expectHabit(*repository.find(habit.identifier), habit);
}

TEST_F(SqliteHabitRepositoryTest, OuterRollbackRestoresHabitAndCascadeDeletedJournal)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    auto habit = sampleHabit();
    habit.identifier = repository.add(habit);
    ASSERT_TRUE(repository.addLog({.habitIdentifier = habit.identifier, .date = kDate, .value = 1.5}));
    EXPECT_THROW(database.transaction([&repository, habit]() {
        auto updated = habit;
        updated.name = "Замена";
        static_cast<void>(repository.update(updated));
        static_cast<void>(repository.addLog({.habitIdentifier = habit.identifier, .date = kDate, .value = 2.0}));
        static_cast<void>(repository.remove(habit.identifier));
        static_cast<void>(repository.add(sampleHabit()));
        throw std::runtime_error{"откат привычки и журнала"};
    }), std::runtime_error);
    ASSERT_EQ(repository.all().size(), 1U);
    expectHabit(*repository.find(habit.identifier), habit);
    ASSERT_EQ(repository.logs(habit.identifier).size(), 1U);
    EXPECT_DOUBLE_EQ(repository.logs(habit.identifier).front().value, 1.5);
    database.transaction([&repository, habit]() { static_cast<void>(repository.remove(habit.identifier)); });
    EXPECT_TRUE(repository.logs(habit.identifier).empty());
}

TEST_F(SqliteHabitRepositoryTest, CascadeFailureDoesNotDeleteHabitAndSqlFailuresPropagate)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    auto habit = sampleHabit();
    habit.identifier = repository.add(habit);
    ASSERT_TRUE(repository.addLog({.habitIdentifier = habit.identifier, .date = kDate, .value = 1.0}));
    database.execute(QStringLiteral("CREATE TRIGGER fail_log_delete BEFORE DELETE ON habit_logs "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.remove(habit.identifier)), std::runtime_error);
    expectHabit(*repository.find(habit.identifier), habit);
    EXPECT_EQ(repository.logs(habit.identifier).size(), 1U);
    database.execute(QStringLiteral("DROP TABLE habit_logs"));
    EXPECT_THROW(static_cast<void>(repository.logs(habit.identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.addLog(
        {.habitIdentifier = habit.identifier, .date = kDate, .value = 1.0})), std::runtime_error);
    database.execute(QStringLiteral("DROP TABLE habits"));
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.find(habit.identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.add(sampleHabit())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.update(habit)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.remove(habit.identifier)), std::runtime_error);
}

TEST_F(SqliteHabitRepositoryTest, SchemaRejectsOrphanLogsAndReadRejectsInvalidCalendarDate)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO habit_logs "
        "(habit_identifier, year, month, day, value) VALUES (999, 2026, 11, 6, 1)")), std::runtime_error);
    const auto identifier = repository.add(sampleHabit());
    ASSERT_TRUE(repository.addLog({.habitIdentifier = identifier, .date = kDate, .value = 1.0}));
    database.execute(QStringLiteral("UPDATE habit_logs SET month = 2, day = 31"));
    EXPECT_THROW(static_cast<void>(repository.logs(identifier)), std::runtime_error);
}

TEST_F(SqliteHabitRepositoryTest, ServiceRestoresDayAndWeekProgressWithoutDeletingOldOrFutureLogs)
{
    core::FakeClock clock{std::chrono::local_days{kDate}};
    std::int64_t dayIdentifier = 0;
    std::int64_t weekIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteHabitRepository repository{database};
        core::HabitService service{clock, repository};
        const auto daily = service.addHabit("День", 1.0, "раз", core::HabitPeriod::Day);
        const auto weekly = service.addHabit("Неделя", 2.0, "раз", core::HabitPeriod::Week);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(daily));
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(weekly));
        dayIdentifier = std::get<std::int64_t>(daily);
        weekIdentifier = std::get<std::int64_t>(weekly);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(service.logHabit(dayIdentifier, 1.5)));
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(service.logHabit(weekIdentifier, 0.5)));
        for (const auto date : std::array{year{2026} / 11 / 2, year{2026} / 11 / 1, year{2026} / 11 / 7}) {
            ASSERT_TRUE(repository.addLog({.habitIdentifier = weekIdentifier, .date = date, .value = 2.0}));
        }
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteHabitRepository repository{database};
    core::HabitService service{clock, repository};
    const auto progress = service.habits();
    ASSERT_EQ(progress.size(), 2U);
    EXPECT_EQ(progress[0].habit.identifier, dayIdentifier);
    EXPECT_DOUBLE_EQ(progress[0].progress, 1.5);
    EXPECT_EQ(progress[1].habit.identifier, weekIdentifier);
    EXPECT_DOUBLE_EQ(progress[1].progress, 2.5);
    EXPECT_EQ(repository.logs(weekIdentifier).size(), 4U);
    clock.advance(std::chrono::days{1});
    const auto tomorrow = service.habits();
    EXPECT_DOUBLE_EQ(tomorrow[0].progress, 0.0);
    EXPECT_DOUBLE_EQ(tomorrow[1].progress, 4.5);
}

}  // namespace
}  // namespace msa::infra

