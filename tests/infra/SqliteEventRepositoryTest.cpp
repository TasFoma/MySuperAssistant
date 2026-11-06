#include "../core/FakeClock.h"

#include "msa/core/CalendarService.h"
#include "msa/core/EventService.h"
#include "msa/core/EventWarningService.h"
#include "msa/infra/SqliteEventRepository.h"
#include "msa/infra/SqliteEventWarningRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteTaskRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace msa::infra {
namespace {

using std::chrono::year;
constexpr auto kDate = year{2028} / 1 / 31;
constexpr std::int64_t kMissingIdentifier = 999999;

[[nodiscard]] core::Event sampleEvent()
{
    return {.title = "  Событие 🙂\n", .date = kDate, .time = std::chrono::minutes{0},
        .description = "  Описание\n'); DELETE FROM events; --  ", .repeat = core::EventRepeat::Monthly};
}

void expectEvent(const core::Event& actual, const core::Event& expected)
{
    EXPECT_EQ(actual.identifier, expected.identifier);
    EXPECT_EQ(actual.title, expected.title);
    EXPECT_EQ(actual.date, expected.date);
    EXPECT_EQ(actual.time, expected.time);
    EXPECT_EQ(actual.description, expected.description);
    EXPECT_EQ(actual.repeat, expected.repeat);
}

class SqliteEventRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-event-test-XXXXXX")};
};

void SqliteEventRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteEventRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("события.sqlite"));
}

TEST_F(SqliteEventRepositoryTest, UpgradesVersionEightAndReopensWithFullFieldsAndPersistentIds)
{
    auto migrations = sqliteMigrations();
    migrations.resize(7);
    {
        SqliteDatabase database{path(), migrations};
        SqliteTaskRepository tasks{database};
        ASSERT_GT(tasks.add({.title = "Прежняя задача"}), 0);
    }
    auto expected = sampleEvent();
    expected.title.append("\0конец", sizeof("\0конец") - 1);
    expected.description.append(20000, 'x');
    expected.description.append("\0конец", sizeof("\0конец") - 1);
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        EXPECT_EQ(database.schemaVersion(), sqliteMigrations().back().version);
        SqliteTaskRepository tasks{database};
        ASSERT_EQ(tasks.all().size(), 1U);
        EXPECT_EQ(tasks.all().front().title, "Прежняя задача");
        SqliteEventRepository repository{database};
        EXPECT_TRUE(repository.all().empty());
        const auto original = expected;
        expected.identifier = repository.add(original);
        EXPECT_EQ(original.identifier, 0);
        EXPECT_GT(expected.identifier, 0);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository repository{database};
    const auto found = repository.find(expected.identifier);
    ASSERT_TRUE(found);
    expectEvent(*found, expected);
    auto copies = repository.all();
    ASSERT_EQ(copies.size(), 1U);
    copies.front().title.clear();
    expectEvent(*repository.find(expected.identifier), expected);
    EXPECT_GT(repository.add(sampleEvent()), expected.identifier);
}

TEST_F(SqliteEventRepositoryTest, UpdatesWholeSeriesIncludingTimeBoundariesAndAllRepeatModes)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository repository{database};
    auto item = sampleEvent();
    item.identifier = repository.add(item);
    for (const auto repeat : std::array{core::EventRepeat::None, core::EventRepeat::Daily,
             core::EventRepeat::Weekly, core::EventRepeat::Monthly}) {
        item.repeat = repeat;
        item.time = std::chrono::minutes{1439};
        ASSERT_TRUE(repository.update(item));
        expectEvent(*repository.find(item.identifier), item);
    }
    item.title = "Изменено";
    item.description.clear();
    item.time.reset();
    item.date = std::chrono::year::min() / 1 / 1;
    ASSERT_TRUE(repository.update(item));
    expectEvent(*repository.find(item.identifier), item);
    item.date = std::chrono::year::max() / 12 / 31;
    ASSERT_TRUE(repository.update(item));
    expectEvent(*repository.find(item.identifier), item);
    EXPECT_FALSE(repository.find(kMissingIdentifier));
    auto missing = item;
    missing.identifier = kMissingIdentifier;
    EXPECT_FALSE(repository.update(missing));
    EXPECT_FALSE(repository.remove(kMissingIdentifier));
    ASSERT_TRUE(repository.remove(item.identifier));
    EXPECT_FALSE(repository.remove(item.identifier));
    EXPECT_FALSE(repository.find(item.identifier));
    EXPECT_GT(repository.add(sampleEvent()), item.identifier);
}

TEST_F(SqliteEventRepositoryTest, InvalidFieldsAndSqlErrorsDoNotChangeExistingEvent)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository repository{database};
    auto item = sampleEvent();
    item.identifier = repository.add(item);
    EXPECT_THROW(static_cast<void>(repository.add(item)), std::invalid_argument);
    auto invalid = item;
    invalid.date = year{2026} / 2 / 29;
    EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    invalid = item;
    invalid.repeat = static_cast<core::EventRepeat>(255);
    EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    for (const auto time : std::array{std::chrono::minutes{-1}, std::chrono::minutes{1440}}) {
        invalid = item;
        invalid.time = time;
        EXPECT_THROW(static_cast<void>(repository.update(invalid)), std::invalid_argument);
    }
    database.execute(QStringLiteral("CREATE TRIGGER fail_event BEFORE UPDATE ON events "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.update(item)), std::runtime_error);
    expectEvent(*repository.find(item.identifier), item);
}

TEST_F(SqliteEventRepositoryTest, ReadErrorsAreNotEmptyResultsAndWritesParticipateInRollback)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository repository{database};
    auto original = sampleEvent();
    original.identifier = repository.add(original);
    EXPECT_THROW(database.transaction([&repository, original]() {
        auto updated = original;
        updated.description = "Замена";
        static_cast<void>(repository.update(updated));
        static_cast<void>(repository.remove(original.identifier));
        static_cast<void>(repository.add(sampleEvent()));
        throw std::runtime_error{"откат событий"};
    }), std::runtime_error);
    ASSERT_EQ(repository.all().size(), 1U);
    expectEvent(*repository.find(original.identifier), original);
    database.execute(QStringLiteral("UPDATE events SET month = 2, day = 31"));
    EXPECT_THROW(static_cast<void>(repository.find(original.identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    database.execute(QStringLiteral("DROP TABLE events"));
    EXPECT_THROW(static_cast<void>(repository.find(original.identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.add(sampleEvent())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.update(original)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.remove(original.identifier)), std::runtime_error);
}

TEST_F(SqliteEventRepositoryTest, CalendarAndServiceKeepOriginalSeriesAndClampShortMonthAfterReopen)
{
    std::int64_t identifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEventRepository repository{database};
        core::EventService service{repository};
        const auto result = service.addEvent("Ежемесячное", kDate, std::nullopt, "Описание",
            core::EventRepeat::Monthly);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result));
        identifier = std::get<std::int64_t>(result);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository repository{database};
    core::CalendarService calendar{repository};
    const auto february = calendar.eventsForDate(year{2028} / 2 / 29);
    ASSERT_EQ(february.size(), 1U);
    EXPECT_EQ(february.front().date, kDate);
    EXPECT_EQ(february.front().identifier, identifier);
    EXPECT_EQ(calendar.eventsForDate(year{2028} / 3 / 31).size(), 1U);
    core::EventService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.updateEvent(identifier, "Разовое",
        year{2028} / 2 / 28, std::chrono::minutes{0}, "", core::EventRepeat::None)));
    EXPECT_TRUE(calendar.eventsForDate(year{2028} / 2 / 29).empty());
    EXPECT_EQ(calendar.eventsForDate(year{2028} / 2 / 28).size(), 1U);
}

TEST_F(SqliteEventRepositoryTest, WarningMarksSurviveSeriesEditReopenAndDeletion)
{
    core::FakeClock clock{std::chrono::local_days{kDate} + std::chrono::hours{8}};
    std::int64_t identifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEventRepository events{database};
        auto item = sampleEvent();
        item.time = std::chrono::minutes{510};
        identifier = events.add(item);
        core::CalendarService calendar{events};
        SqliteEventWarningRepository marks{database};
        core::EventWarningService service{clock, calendar, marks};
        EXPECT_EQ(service.checkDue().size(), 1U);
        item.identifier = identifier;
        item.time = std::chrono::minutes{525};
        ASSERT_TRUE(events.update(item));
        EXPECT_TRUE(service.checkDue().empty());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventRepository events{database};
    core::CalendarService calendar{events};
    SqliteEventWarningRepository marks{database};
    core::EventWarningService service{clock, calendar, marks};
    EXPECT_TRUE(service.checkDue().empty());
    ASSERT_TRUE(events.remove(identifier));
    EXPECT_FALSE(marks.markIfNew(identifier, kDate, core::EventWarningKind::BeforeStart));
    EXPECT_GT(events.add(sampleEvent()), identifier);
}

}  // namespace
}  // namespace msa::infra

