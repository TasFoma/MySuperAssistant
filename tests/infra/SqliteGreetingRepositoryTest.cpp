#include "../core/FakeClock.h"
#include "../core/InMemoryEventRepository.h"
#include "../core/InMemoryTaskRepository.h"

#include "msa/core/CalendarService.h"
#include "msa/core/GreetingService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteGreetingRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteUserProfileRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace msa::infra {
namespace {

using std::chrono::year;
constexpr auto kDate = year{2026} / 11 / 6;
constexpr auto kNextDate = year{2026} / 11 / 7;

class SqliteGreetingRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-greeting-test-XXXXXX")};
};

void SqliteGreetingRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteGreetingRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("приветствие.sqlite"));
}

TEST_F(SqliteGreetingRepositoryTest, UpgradesVersionThreeKeepingChatsAndProfileAndStartsWithoutDate)
{
    auto migrations = sqliteMigrations();
    migrations.resize(2);
    std::int64_t chatIdentifier = 0;
    const core::UserProfile profile{.facts = {{.text = "Факт", .identifier = 1}}};
    {
        SqliteDatabase database{path(), migrations};
        SqliteChatRepository chats{database};
        chatIdentifier = chats.addChat({.title = "Чат"});
        ASSERT_TRUE(chats.saveCurrentChatId(chatIdentifier));
        SqliteUserProfileRepository profiles{database};
        profiles.save(profile);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository greetings{database};
    EXPECT_EQ(database.schemaVersion(), 4);
    EXPECT_FALSE(greetings.lastDate());
    SqliteChatRepository chats{database};
    EXPECT_EQ(chats.currentChatId(), chatIdentifier);
    EXPECT_TRUE(chats.findChat(chatIdentifier));
    SqliteUserProfileRepository profiles{database};
    EXPECT_EQ(profiles.load().facts, profile.facts);
}

TEST_F(SqliteGreetingRepositoryTest, ReopensAndReplacesCalendarDateIncludingLeapDayAndEarlierDates)
{
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteGreetingRepository repository{database};
        repository.saveDate(kDate);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository repository{database};
    EXPECT_EQ(repository.lastDate(), kDate);
    const std::array dates{year{2028} / 2 / 29, year{2027} / 1 / 1, year{1969} / 12 / 31,
        year{0} / 1 / 1, std::chrono::year::min() / 1 / 1, std::chrono::year::max() / 12 / 31};
    for (const auto date : dates) {
        repository.saveDate(date);
        EXPECT_EQ(repository.lastDate(), date);
        repository.saveDate(date);
        EXPECT_EQ(repository.lastDate(), date);
    }
}

TEST_F(SqliteGreetingRepositoryTest, RejectsInvalidDateWithoutChangingPersistedState)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository repository{database};
    EXPECT_THROW(repository.saveDate(year{2026} / 2 / 29), std::invalid_argument);
    EXPECT_FALSE(repository.lastDate());
    repository.saveDate(kDate);
    EXPECT_THROW(repository.saveDate(year{2026} / 13 / 1), std::invalid_argument);
    EXPECT_EQ(repository.lastDate(), kDate);
    database.execute(QStringLiteral("UPDATE greeting_state SET month = 2, day = 31"));
    EXPECT_THROW(static_cast<void>(repository.lastDate()), std::runtime_error);
}

TEST_F(SqliteGreetingRepositoryTest, JoinsOuterTransactionAndRestoresDateOnRollback)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository repository{database};
    EXPECT_THROW(database.transaction([&repository]() {
        repository.saveDate(kDate);
        throw std::runtime_error{"откат первой записи"};
    }), std::runtime_error);
    EXPECT_FALSE(repository.lastDate());
    repository.saveDate(kDate);
    EXPECT_THROW(database.transaction([&repository]() {
        repository.saveDate(kNextDate);
        throw std::runtime_error{"откат замены"};
    }), std::runtime_error);
    EXPECT_EQ(repository.lastDate(), kDate);
    database.transaction([&repository]() { repository.saveDate(kNextDate); });
    EXPECT_EQ(repository.lastDate(), kNextDate);
}

TEST_F(SqliteGreetingRepositoryTest, WriteFailurePreservesDateAndReadFailureIsNotEmptyState)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository repository{database};
    repository.saveDate(kDate);
    database.execute(QStringLiteral("CREATE TRIGGER fail_greeting BEFORE UPDATE ON greeting_state "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(repository.saveDate(kNextDate), std::runtime_error);
    EXPECT_EQ(repository.lastDate(), kDate);
    database.execute(QStringLiteral("DROP TABLE greeting_state"));
    EXPECT_THROW(static_cast<void>(repository.lastDate()), std::runtime_error);
    EXPECT_THROW(repository.saveDate(kDate), std::runtime_error);
}

TEST_F(SqliteGreetingRepositoryTest, ServiceDoesNotRepeatAfterReopenAndGreetsOnNextLocalDay)
{
    core::FakeClock clock{std::chrono::local_days{year{2028} / 2 / 28} + std::chrono::hours{23}};
    const core::InMemoryEventRepository events;
    const core::InMemoryTaskRepository tasks;
    const core::CalendarService calendar{events};
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteGreetingRepository repository{database};
        core::GreetingService service{clock, calendar, tasks, repository};
        EXPECT_TRUE(service.greetingIfNeeded());
        EXPECT_FALSE(service.greetingIfNeeded());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteGreetingRepository repository{database};
    core::GreetingService service{clock, calendar, tasks, repository};
    EXPECT_FALSE(service.greetingIfNeeded());
    clock.advance(std::chrono::hours{1});
    EXPECT_TRUE(service.greetingIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2028} / 2 / 29);
    EXPECT_FALSE(service.greetingIfNeeded());
}

}  // namespace
}  // namespace msa::infra

