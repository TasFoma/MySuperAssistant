#include "FakeClock.h"
#include "InMemoryEventRepository.h"

#include "msa/core/CalendarService.h"
#include "msa/core/EventWarningService.h"
#include "msa/infra/SqliteEveningCheckInRepository.h"
#include "msa/infra/SqliteEventWarningRepository.h"
#include "msa/infra/SqliteSchema.h"

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
constexpr std::int64_t kEventIdentifier = 1234567890123;

class SqliteEventWarningRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-warning-test-XXXXXX")};
};

void SqliteEventWarningRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteEventWarningRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("предупреждения.sqlite"));
}

TEST_F(SqliteEventWarningRepositoryTest, UpgradesVersionFiveAndKeepsNewMarkAcrossReopen)
{
    auto migrations = sqliteMigrations();
    migrations.resize(4);
    {
        SqliteDatabase database{path(), migrations};
        SqliteEveningCheckInRepository checkIns{database};
        checkIns.saveDate(kDate);
    }
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        EXPECT_EQ(database.schemaVersion(), sqliteMigrations().back().version);
        SqliteEveningCheckInRepository checkIns{database};
        EXPECT_EQ(checkIns.lastDate(), kDate);
        SqliteEventWarningRepository repository{database};
        EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::BeforeStart));
        EXPECT_FALSE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::BeforeStart));
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    EXPECT_FALSE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::BeforeStart));
}

TEST_F(SqliteEventWarningRepositoryTest, DistinguishesAllPartsOfKeyAndKeepsCalendarDates)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    for (const auto kind : std::array{core::EventWarningKind::BeforeStart,
             core::EventWarningKind::DayBefore, core::EventWarningKind::OnDay}) {
        EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, kind));
        EXPECT_FALSE(repository.markIfNew(kEventIdentifier, kDate, kind));
    }
    EXPECT_TRUE(repository.markIfNew(kEventIdentifier + 1, kDate, core::EventWarningKind::OnDay));
    for (const auto date : std::array{kNextDate, year{2028} / 2 / 29, year{1969} / 12 / 31,
             std::chrono::year::min() / 1 / 1, std::chrono::year::max() / 12 / 31}) {
        EXPECT_TRUE(repository.markIfNew(kEventIdentifier, date, core::EventWarningKind::OnDay));
        EXPECT_FALSE(repository.markIfNew(kEventIdentifier, date, core::EventWarningKind::OnDay));
    }
}

TEST_F(SqliteEventWarningRepositoryTest, InvalidInputDoesNotCreateMarks)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    EXPECT_THROW(static_cast<void>(repository.markIfNew(0, kDate,
        core::EventWarningKind::OnDay)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.markIfNew(-1, kDate,
        core::EventWarningKind::OnDay)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.markIfNew(kEventIdentifier, year{2026} / 2 / 29,
        core::EventWarningKind::OnDay)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.markIfNew(kEventIdentifier, kDate,
        static_cast<core::EventWarningKind>(255))), std::invalid_argument);
    EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::OnDay));
}

TEST_F(SqliteEventWarningRepositoryTest, MarksJoinOuterTransactionAndRollbackAllowsRetry)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    EXPECT_THROW(database.transaction([&repository]() {
        EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::OnDay));
        throw std::runtime_error{"откат отметки"};
    }), std::runtime_error);
    EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::OnDay));
    database.transaction([&repository]() {
        EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kNextDate, core::EventWarningKind::OnDay));
    });
    EXPECT_FALSE(repository.markIfNew(kEventIdentifier, kNextDate, core::EventWarningKind::OnDay));
}

TEST_F(SqliteEventWarningRepositoryTest, SqlFailureIsNotReportedAsDuplicate)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    database.execute(QStringLiteral("CREATE TRIGGER fail_mark BEFORE INSERT ON event_warning_marks "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.markIfNew(kEventIdentifier, kDate,
        core::EventWarningKind::OnDay)), std::runtime_error);
    database.execute(QStringLiteral("DROP TRIGGER fail_mark"));
    EXPECT_TRUE(repository.markIfNew(kEventIdentifier, kDate, core::EventWarningKind::OnDay));
    database.execute(QStringLiteral("DROP TABLE event_warning_marks"));
    EXPECT_THROW(static_cast<void>(repository.markIfNew(kEventIdentifier, kDate,
        core::EventWarningKind::OnDay)), std::runtime_error);
}

TEST_F(SqliteEventWarningRepositoryTest, ServiceDoesNotRepeatAfterReopenButAllowsNewOccurrence)
{
    core::FakeClock clock{std::chrono::local_days{kDate} + std::chrono::hours{8}};
    core::InMemoryEventRepository events;
    static_cast<void>(events.add({.title = "Ежедневное событие", .date = kDate,
        .repeat = core::EventRepeat::Daily}));
    const core::CalendarService calendar{events};
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteEventWarningRepository repository{database};
        core::EventWarningService service{clock, calendar, repository};
        EXPECT_EQ(service.checkDue().size(), 2U);
        EXPECT_TRUE(service.checkDue().empty());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteEventWarningRepository repository{database};
    core::EventWarningService service{clock, calendar, repository};
    EXPECT_TRUE(service.checkDue().empty());
    clock.advance(std::chrono::days{1});
    EXPECT_EQ(service.checkDue().size(), 2U);
    EXPECT_TRUE(service.checkDue().empty());
}

}  // namespace
}  // namespace msa::infra

