#include "../core/FakeClock.h"

#include "msa/core/EveningCheckInService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteEveningCheckInRepository.h"
#include "msa/infra/SqliteGreetingRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteUserProfileRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <optional>
#include <stdexcept>

namespace msa::infra {
namespace {

using std::chrono::year;
constexpr auto kDate = year{2026} / 11 / 6;
constexpr auto kNextDate = year{2026} / 11 / 7;
constexpr std::int64_t kMissingIdentifier = 999999;

[[nodiscard]] core::PendingEveningCheckIn createPrompt(SqliteDatabase& database)
{
    SqliteChatRepository chats{database};
    const auto chatIdentifier = chats.addChat({.title = "Опрос"});
    const auto messageIdentifier = chats.addMessage({.chatIdentifier = chatIdentifier,
        .role = core::MessageRole::Assistant, .text = "Как прошёл день?"});
    if (!messageIdentifier) {
        throw std::runtime_error{"Не создан тестовый опрос"};
    }

    return {.chatIdentifier = chatIdentifier, .messageIdentifier = *messageIdentifier, .date = kDate};
}

void expectPending(const std::optional<core::PendingEveningCheckIn>& actual,
    const core::PendingEveningCheckIn& expected)
{
    ASSERT_TRUE(actual);
    EXPECT_EQ(actual->chatIdentifier, expected.chatIdentifier);
    EXPECT_EQ(actual->messageIdentifier, expected.messageIdentifier);
    EXPECT_EQ(actual->date, expected.date);
}

class SqliteEveningCheckInRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-check-in-test-XXXXXX")};
};

void SqliteEveningCheckInRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteEveningCheckInRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("вечерний опрос.sqlite"));
}

TEST_F(SqliteEveningCheckInRepositoryTest, UpgradesVersionFourKeepingExistingDataAndStartsEmpty)
{
    auto migrations = sqliteMigrations();
    migrations.resize(3);
    core::PendingEveningCheckIn prompt;
    const core::UserProfile profile{.facts = {{.text = "Факт", .identifier = 1}}};
    {
        SqliteDatabase database{path(), migrations};
        prompt = createPrompt(database);
        SqliteGreetingRepository greetings{database};
        greetings.saveDate(kDate);
        SqliteUserProfileRepository profiles{database};
        profiles.save(profile);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    EXPECT_EQ(database.schemaVersion(), 5);
    SqliteEveningCheckInRepository repository{database};
    EXPECT_FALSE(repository.lastDate());
    EXPECT_FALSE(repository.pendingCheckIn());
    SqliteGreetingRepository greetings{database};
    EXPECT_EQ(greetings.lastDate(), kDate);
    SqliteUserProfileRepository profiles{database};
    EXPECT_EQ(profiles.load().facts, profile.facts);
    SqliteChatRepository chats{database};
    ASSERT_EQ(chats.messages(prompt.chatIdentifier).size(), 1U);
    EXPECT_EQ(chats.messages(prompt.chatIdentifier).front().identifier, prompt.messageIdentifier);
}

TEST_F(SqliteEveningCheckInRepositoryTest, ReopensWithDateAndFullPendingStateAndClearsOnlyPending)
{
    core::PendingEveningCheckIn prompt;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEveningCheckInRepository repository{database};
        prompt = createPrompt(database);
        repository.saveDate(kDate);
        repository.savePendingCheckIn(prompt);
    }
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEveningCheckInRepository repository{database};
        EXPECT_EQ(repository.lastDate(), kDate);
        expectPending(repository.pendingCheckIn(), prompt);
        auto copy = repository.pendingCheckIn();
        ASSERT_TRUE(copy);
        copy->messageIdentifier = kMissingIdentifier;
        expectPending(repository.pendingCheckIn(), prompt);
        repository.savePendingCheckIn(std::nullopt);
        repository.savePendingCheckIn(std::nullopt);
        EXPECT_FALSE(repository.pendingCheckIn());
        EXPECT_EQ(repository.lastDate(), kDate);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    EXPECT_FALSE(repository.pendingCheckIn());
    EXPECT_EQ(repository.lastDate(), kDate);
}

TEST_F(SqliteEveningCheckInRepositoryTest, ReplacesPendingAndKeepsDatesAndGreetingIndependent)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    SqliteGreetingRepository greetings{database};
    greetings.saveDate(kDate);
    const auto first = createPrompt(database);
    auto second = createPrompt(database);
    second.date = kNextDate;
    repository.savePendingCheckIn(first);
    EXPECT_FALSE(repository.lastDate());
    repository.saveDate(kNextDate);
    expectPending(repository.pendingCheckIn(), first);
    repository.savePendingCheckIn(second);
    expectPending(repository.pendingCheckIn(), second);
    repository.saveDate(kDate);
    EXPECT_EQ(repository.lastDate(), kDate);
    expectPending(repository.pendingCheckIn(), second);
    repository.savePendingCheckIn(std::nullopt);
    EXPECT_EQ(greetings.lastDate(), kDate);
    EXPECT_EQ(repository.lastDate(), kDate);
}

TEST_F(SqliteEveningCheckInRepositoryTest, PreservesCalendarDatesAndRejectsInvalidInputWithoutChanges)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    const auto prompt = createPrompt(database);
    const std::array dates{year{2028} / 2 / 29, year{1969} / 12 / 31,
        std::chrono::year::min() / 1 / 1, std::chrono::year::max() / 12 / 31};
    for (const auto date : dates) {
        repository.saveDate(date);
        auto updated = prompt;
        updated.date = date;
        repository.savePendingCheckIn(updated);
        EXPECT_EQ(repository.lastDate(), date);
        expectPending(repository.pendingCheckIn(), updated);
    }
    repository.saveDate(kDate);
    repository.savePendingCheckIn(prompt);
    EXPECT_THROW(repository.saveDate(year{2026} / 2 / 29), std::invalid_argument);
    auto invalid = prompt;
    invalid.date = year{2026} / 2 / 29;
    EXPECT_THROW(repository.savePendingCheckIn(invalid), std::invalid_argument);
    invalid = prompt;
    invalid.chatIdentifier = 0;
    EXPECT_THROW(repository.savePendingCheckIn(invalid), std::invalid_argument);
    invalid = prompt;
    invalid.messageIdentifier = -1;
    EXPECT_THROW(repository.savePendingCheckIn(invalid), std::invalid_argument);
    EXPECT_EQ(repository.lastDate(), kDate);
    expectPending(repository.pendingCheckIn(), prompt);
}

TEST_F(SqliteEveningCheckInRepositoryTest, RejectsMissingOrMismatchedMessageWithoutReplacingPending)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    const auto prompt = createPrompt(database);
    repository.savePendingCheckIn(prompt);
    auto invalid = prompt;
    invalid.messageIdentifier = kMissingIdentifier;
    EXPECT_THROW(repository.savePendingCheckIn(invalid), std::runtime_error);
    invalid = prompt;
    invalid.chatIdentifier = createPrompt(database).chatIdentifier;
    EXPECT_THROW(repository.savePendingCheckIn(invalid), std::runtime_error);
    expectPending(repository.pendingCheckIn(), prompt);
}

TEST_F(SqliteEveningCheckInRepositoryTest, OuterTransactionRollsBackPublicationAndRestoresClearedPending)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    SqliteChatRepository chats{database};
    EXPECT_THROW(database.transaction([&database, &repository]() {
        const auto prompt = createPrompt(database);
        repository.saveDate(kDate);
        repository.savePendingCheckIn(prompt);
        throw std::runtime_error{"сбой публикации"};
    }), std::runtime_error);
    EXPECT_FALSE(repository.lastDate());
    EXPECT_FALSE(repository.pendingCheckIn());
    EXPECT_TRUE(chats.chats().empty());
    const auto prompt = createPrompt(database);
    database.transaction([&repository, prompt]() {
        repository.saveDate(kDate);
        repository.savePendingCheckIn(prompt);
    });
    EXPECT_THROW(database.transaction([&repository]() {
        repository.saveDate(kNextDate);
        repository.savePendingCheckIn(std::nullopt);
        throw std::runtime_error{"сбой обработки ответа"};
    }), std::runtime_error);
    EXPECT_EQ(repository.lastDate(), kDate);
    expectPending(repository.pendingCheckIn(), prompt);
}

TEST_F(SqliteEveningCheckInRepositoryTest, PropagatesReadAndWriteErrorsWithoutEmptyFallback)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    const auto prompt = createPrompt(database);
    repository.saveDate(kDate);
    repository.savePendingCheckIn(prompt);
    database.execute(QStringLiteral("CREATE TRIGGER fail_pending BEFORE DELETE ON pending_evening_check_in "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(repository.savePendingCheckIn(std::nullopt), std::runtime_error);
    expectPending(repository.pendingCheckIn(), prompt);
    database.execute(QStringLiteral("UPDATE evening_check_in_state SET month = 2, day = 31"));
    EXPECT_THROW(static_cast<void>(repository.lastDate()), std::runtime_error);
    database.execute(QStringLiteral("UPDATE pending_evening_check_in SET month = 2, day = 31"));
    EXPECT_THROW(static_cast<void>(repository.pendingCheckIn()), std::runtime_error);
    database.execute(QStringLiteral("DROP TABLE pending_evening_check_in"));
    EXPECT_THROW(static_cast<void>(repository.pendingCheckIn()), std::runtime_error);
    EXPECT_THROW(repository.savePendingCheckIn(prompt), std::runtime_error);
    EXPECT_THROW(repository.savePendingCheckIn(std::nullopt), std::runtime_error);
    database.execute(QStringLiteral("DROP TABLE evening_check_in_state"));
    EXPECT_THROW(static_cast<void>(repository.lastDate()), std::runtime_error);
    EXPECT_THROW(repository.saveDate(kDate), std::runtime_error);
}

TEST_F(SqliteEveningCheckInRepositoryTest, ServiceSuppressesRepeatAfterReopenAndRepositoryDoesNotExpirePending)
{
    core::FakeClock clock{std::chrono::local_days{kDate} + std::chrono::hours{20}};
    core::PendingEveningCheckIn prompt;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEveningCheckInRepository repository{database};
        core::EveningCheckInService service{clock, repository};
        EXPECT_TRUE(service.checkInIfNeeded());
        prompt = createPrompt(database);
        repository.savePendingCheckIn(prompt);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEveningCheckInRepository repository{database};
    core::EveningCheckInService service{clock, repository};
    EXPECT_FALSE(service.checkInIfNeeded());
    expectPending(repository.pendingCheckIn(), prompt);
    clock.advance(std::chrono::hours{4});
    EXPECT_FALSE(service.checkInIfNeeded());
    expectPending(repository.pendingCheckIn(), prompt);
    clock.advance(std::chrono::hours{20});
    EXPECT_TRUE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), kNextDate);
    // Репозиторий не подменяет правила истечения из ConversationService.
    expectPending(repository.pendingCheckIn(), prompt);
}

}  // namespace
}  // namespace msa::infra

