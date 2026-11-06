#include "../core/FakeClock.h"

#include "msa/core/TaskService.h"
#include "msa/infra/SqliteMoodRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteTaskRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace msa::infra {
namespace {

using std::chrono::year;
constexpr auto kDate = year{2028} / 2 / 29;
constexpr auto kCreatedAt = std::chrono::local_seconds{std::chrono::seconds{-123456789}};
constexpr std::int64_t kMissingIdentifier = 999999;

[[nodiscard]] core::Task sampleTask()
{
    return {.title = "  Задача\n🙂 '); DELETE FROM tasks; --  ", .dueDate = kDate,
        .priority = core::TaskPriority::High, .done = true, .createdAt = kCreatedAt};
}

void expectTask(const core::Task& actual, const core::Task& expected)
{
    EXPECT_EQ(actual.identifier, expected.identifier);
    EXPECT_EQ(actual.title, expected.title);
    EXPECT_EQ(actual.dueDate, expected.dueDate);
    EXPECT_EQ(actual.priority, expected.priority);
    EXPECT_EQ(actual.done, expected.done);
    EXPECT_EQ(actual.createdAt, expected.createdAt);
}

class SqliteTaskRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-task-test-XXXXXX")};
};

void SqliteTaskRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteTaskRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("задачи.sqlite"));
}

TEST_F(SqliteTaskRepositoryTest, UpgradesVersionSevenAndReopensWithExactFieldsAndPersistentIds)
{
    auto migrations = sqliteMigrations();
    migrations.resize(6);
    {
        SqliteDatabase database{path(), migrations};
        SqliteMoodRepository moods{database};
        ASSERT_GT(moods.add({.note = "Дневник до обновления"}), 0);
    }
    auto expected = sampleTask();
    expected.title.append(20000, 'x');
    expected.title.append("\0конец", sizeof("\0конец") - 1);
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        EXPECT_EQ(database.schemaVersion(), sqliteMigrations().back().version);
        SqliteMoodRepository moods{database};
        ASSERT_EQ(moods.all().size(), 1U);
        EXPECT_EQ(moods.all().front().note, "Дневник до обновления");
        SqliteTaskRepository repository{database};
        EXPECT_TRUE(repository.all().empty());
        const auto original = expected;
        expected.identifier = repository.add(original);
        EXPECT_EQ(original.identifier, 0);
        ASSERT_GT(expected.identifier, 0);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    const auto found = repository.find(expected.identifier);
    ASSERT_TRUE(found);
    expectTask(*found, expected);
    EXPECT_GT(repository.add(sampleTask()), expected.identifier);
    EXPECT_EQ(repository.all().size(), 2U);
    auto copies = repository.all();
    copies.front().title.clear();
    expectTask(*repository.find(expected.identifier), expected);
}

TEST_F(SqliteTaskRepositoryTest, UpdatesAllFieldsIncludingNullableDateAndDeletesWithoutReusingId)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    auto item = sampleTask();
    item.identifier = repository.add(item);
    EXPECT_TRUE(repository.update(item));
    item.title = "Новое название";
    item.dueDate.reset();
    item.priority = core::TaskPriority::Low;
    item.done = false;
    item.createdAt += std::chrono::hours{1};
    ASSERT_TRUE(repository.update(item));
    expectTask(*repository.find(item.identifier), item);
    item.dueDate = std::chrono::year::min() / 1 / 1;
    item.priority = core::TaskPriority::Normal;
    ASSERT_TRUE(repository.update(item));
    expectTask(*repository.find(item.identifier), item);
    EXPECT_FALSE(repository.find(kMissingIdentifier));
    auto missing = item;
    missing.identifier = kMissingIdentifier;
    EXPECT_FALSE(repository.update(missing));
    EXPECT_FALSE(repository.remove(kMissingIdentifier));
    ASSERT_TRUE(repository.remove(item.identifier));
    EXPECT_FALSE(repository.remove(item.identifier));
    EXPECT_FALSE(repository.find(item.identifier));
    EXPECT_TRUE(repository.all().empty());
    EXPECT_GT(repository.add(sampleTask()), item.identifier);
}

TEST_F(SqliteTaskRepositoryTest, InvalidInputAndWriteFailureLeaveExistingTaskIntact)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    auto item = sampleTask();
    item.identifier = repository.add(item);
    EXPECT_THROW(static_cast<void>(repository.add(item)), std::invalid_argument);
    auto invalid = item;
    invalid.dueDate = year{2026} / 2 / 29;
    EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    invalid = item;
    invalid.priority = static_cast<core::TaskPriority>(255);
    EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    database.execute(QStringLiteral("CREATE TRIGGER fail_task BEFORE UPDATE ON tasks "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.update(item)), std::runtime_error);
    expectTask(*repository.find(item.identifier), item);
    EXPECT_EQ(repository.all().size(), 1U);
}

TEST_F(SqliteTaskRepositoryTest, OuterTransactionRollsBackAddUpdateAndRemove)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    auto original = sampleTask();
    original.identifier = repository.add(original);
    EXPECT_THROW(database.transaction([&repository, original]() {
        auto updated = original;
        updated.title = "Изменено";
        static_cast<void>(repository.update(updated));
        static_cast<void>(repository.remove(original.identifier));
        static_cast<void>(repository.add(sampleTask()));
        throw std::runtime_error{"откат задач"};
    }), std::runtime_error);
    ASSERT_EQ(repository.all().size(), 1U);
    expectTask(*repository.find(original.identifier), original);
    database.transaction([&repository, original]() {
        static_cast<void>(repository.remove(original.identifier));
    });
    EXPECT_TRUE(repository.all().empty());
}

TEST_F(SqliteTaskRepositoryTest, SqlReadErrorsAndCorruptDateAreNotReportedAsMissingTasks)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    const auto identifier = repository.add(sampleTask());
    database.execute(QStringLiteral("UPDATE tasks SET due_year = 2026, due_month = 2, due_day = 31"));
    EXPECT_THROW(static_cast<void>(repository.find(identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    database.execute(QStringLiteral("DROP TABLE tasks"));
    EXPECT_THROW(static_cast<void>(repository.find(identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.add(sampleTask())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.update(sampleTask())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.remove(identifier)), std::runtime_error);
}

TEST_F(SqliteTaskRepositoryTest, ServicePreservesCreationTimeAndStatusAndSortsPersistedTasks)
{
    core::FakeClock clock{std::chrono::local_days{year{2026} / 11 / 6}};
    std::int64_t highIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteTaskRepository repository{database};
        core::TaskService service{clock, repository};
        const auto high = service.addTask("Важная", year{2026} / 11 / 5, core::TaskPriority::High);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(high));
        highIdentifier = std::get<std::int64_t>(high);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(
            service.addTask("Без срока", std::nullopt, core::TaskPriority::Normal)));
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(
            service.addTask("Со сроком", kDate, core::TaskPriority::Normal)));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(service.setTaskDone(highIdentifier, true)));
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteTaskRepository repository{database};
    core::TaskService service{clock, repository};
    clock.advance(std::chrono::days{1});
    ASSERT_TRUE(std::holds_alternative<std::monostate>(
        service.updateTask(highIdentifier, "Изменена", std::nullopt, core::TaskPriority::High)));
    const auto found = repository.find(highIdentifier);
    ASSERT_TRUE(found);
    EXPECT_TRUE(found->done);
    EXPECT_EQ(found->createdAt, clock.now() - std::chrono::days{1});
    const auto tasks = service.tasks();
    ASSERT_EQ(tasks.size(), 3U);
    EXPECT_EQ(tasks[0].title, "Со сроком");
    EXPECT_EQ(tasks[1].title, "Без срока");
    EXPECT_EQ(tasks[2].identifier, highIdentifier);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.setTaskDone(highIdentifier, false)));
    EXPECT_EQ(service.tasks().front().identifier, highIdentifier);
}

}  // namespace
}  // namespace msa::infra

