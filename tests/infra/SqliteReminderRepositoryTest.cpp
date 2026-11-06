#include "../core/FakeClock.h"

#include "msa/core/ReminderService.h"
#include "msa/infra/SqliteEventRepository.h"
#include "msa/infra/SqliteReminderRepository.h"
#include "msa/infra/SqliteSchema.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace msa::infra {
namespace {

constexpr auto kMoment = std::chrono::local_seconds{std::chrono::seconds{-123456789}};
constexpr std::int64_t kMissingIdentifier = 999999;

void setIdentifier(core::Reminder& reminder, std::int64_t identifier)
{
    std::visit([identifier](auto& item) { item.identifier = identifier; }, reminder);
}

[[nodiscard]] std::int64_t identifierOf(const core::Reminder& reminder)
{
    return std::visit([](const auto& item) { return item.identifier; }, reminder);
}

void expectReminder(const core::Reminder& actual, const core::Reminder& expected)
{
    ASSERT_EQ(actual.index(), expected.index());
    std::visit([&actual](const auto& item) {
        using ReminderType = std::decay_t<decltype(item)>;
        const auto& loaded = std::get<ReminderType>(actual);
        EXPECT_EQ(loaded.identifier, item.identifier);
        EXPECT_EQ(loaded.title, item.title);
        if constexpr (std::is_same_v<ReminderType, core::PlaceReminder>) {
            EXPECT_EQ(loaded.place, item.place);
            ASSERT_EQ(loaded.coordinates.has_value(), item.coordinates.has_value());
            if (item.coordinates) {
                EXPECT_DOUBLE_EQ(loaded.coordinates->latitude, item.coordinates->latitude);
                EXPECT_DOUBLE_EQ(loaded.coordinates->longitude, item.coordinates->longitude);
            }
        } else {
            EXPECT_EQ(loaded.dueAt, item.dueAt);
            if constexpr (std::is_same_v<ReminderType, core::RecurringReminder>) {
                EXPECT_EQ(loaded.interval, item.interval);
            }
        }
        if constexpr (!std::is_same_v<ReminderType, core::RecurringReminder>) {
            EXPECT_EQ(loaded.status, item.status);
        }
    }, expected);
}

[[nodiscard]] std::vector<core::Reminder> examples()
{
    std::string text = "  Напоминание\n🙂 '); DELETE FROM reminders; --  ";
    text.append(20000, 'x');
    text.append("\0конец", sizeof("\0конец") - 1);

    return {core::TimedReminder{.title = text, .dueAt = kMoment},
        core::TimedReminder{.title = "Показано", .dueAt = kMoment, .status = core::ReminderStatus::Fired},
        core::TimedReminder{.title = "Пропущено", .dueAt = kMoment, .status = core::ReminderStatus::Missed},
        core::RecurringReminder{.title = text, .dueAt = kMoment, .interval = std::chrono::minutes{15}},
        core::PlaceReminder{.title = text, .place = text},
        core::PlaceReminder{.title = "Нули", .place = "Экватор", .coordinates = core::GeoCoordinates{0.0, 0.0},
            .status = core::ReminderStatus::Fired},
        core::PlaceReminder{.title = "Граница", .place = "Полюс", .coordinates = core::GeoCoordinates{-90.0, 180.0},
            .status = core::ReminderStatus::Missed}};
}

class SqliteReminderRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-reminder-test-XXXXXX")};
};

void SqliteReminderRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteReminderRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("напоминания.sqlite"));
}

TEST_F(SqliteReminderRepositoryTest, UpgradesVersionNineWithoutChangingEventsAndStartsEmpty)
{
    auto migrations = sqliteMigrations();
    migrations.resize(8);
    {
        SqliteDatabase database{path(), migrations};
        SqliteEventRepository events{database};
        ASSERT_GT(events.add({.title = "Событие", .date = std::chrono::year{2026} / 11 / 6}), 0);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    EXPECT_EQ(database.schemaVersion(), 10);
    SqliteEventRepository events{database};
    ASSERT_EQ(events.all().size(), 1U);
    EXPECT_EQ(events.all().front().title, "Событие");
    SqliteReminderRepository repository{database};
    EXPECT_TRUE(repository.all().empty());
    EXPECT_FALSE(repository.find(kMissingIdentifier));
}

TEST_F(SqliteReminderRepositoryTest, ReopensAllVariantsWithExactFieldsSharedIdsAndIndependentCopies)
{
    auto expected = examples();
    std::int64_t lastIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteReminderRepository repository{database};
        for (auto& reminder : expected) {
            const auto identifier = repository.add(reminder);
            EXPECT_GT(identifier, lastIdentifier);
            EXPECT_EQ(identifierOf(reminder), 0);
            setIdentifier(reminder, identifier);
            lastIdentifier = identifier;
        }
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    EXPECT_EQ(repository.all().size(), expected.size());
    for (const auto& reminder : expected) {
        const auto found = repository.find(identifierOf(reminder));
        ASSERT_TRUE(found);
        expectReminder(*found, reminder);
    }
    auto copies = repository.all();
    std::visit([](auto& item) { item.title.clear(); }, copies.front());
    expectReminder(*repository.find(identifierOf(expected.front())), expected.front());
    EXPECT_GT(repository.add(examples().front()), lastIdentifier);
}

TEST_F(SqliteReminderRepositoryTest, ReplacesVariantsClearsUnusedFieldsAndDeletesWithoutReusingId)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    const auto identifier = repository.add(examples().front());
    for (auto reminder : examples()) {
        setIdentifier(reminder, identifier);
        ASSERT_TRUE(repository.update(reminder));
        expectReminder(*repository.find(identifier), reminder);
    }
    auto missing = examples().front();
    setIdentifier(missing, kMissingIdentifier);
    EXPECT_FALSE(repository.update(missing));
    EXPECT_FALSE(repository.remove(kMissingIdentifier));
    ASSERT_TRUE(repository.remove(identifier));
    EXPECT_FALSE(repository.remove(identifier));
    EXPECT_FALSE(repository.find(identifier));
    EXPECT_TRUE(repository.all().empty());
    EXPECT_GT(repository.add(examples().front()), identifier);
}

TEST_F(SqliteReminderRepositoryTest, RejectsInvalidStatusIntervalCoordinatesAndNonzeroNewId)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    const std::vector<core::Reminder> invalid{
        core::TimedReminder{.identifier = 1},
        core::TimedReminder{.status = static_cast<core::ReminderStatus>(255)},
        core::RecurringReminder{.interval = std::chrono::minutes{0}},
        core::RecurringReminder{.interval = std::chrono::minutes{-1}},
        core::PlaceReminder{.coordinates = core::GeoCoordinates{91.0, 0.0}},
        core::PlaceReminder{.coordinates = core::GeoCoordinates{0.0, -181.0}},
        core::PlaceReminder{.coordinates = core::GeoCoordinates{std::numeric_limits<double>::quiet_NaN(), 0.0}},
        core::PlaceReminder{.coordinates = core::GeoCoordinates{0.0, std::numeric_limits<double>::infinity()}}};
    for (const auto& reminder : invalid) {
        EXPECT_THROW(static_cast<void>(repository.add(reminder)), std::invalid_argument);
    }
    EXPECT_TRUE(repository.all().empty());
    const auto identifier = repository.add(examples().front());
    auto badUpdate = invalid.back();
    setIdentifier(badUpdate, identifier);
    EXPECT_THROW(static_cast<void>(repository.update(badUpdate)), std::invalid_argument);
    EXPECT_TRUE(std::holds_alternative<core::TimedReminder>(*repository.find(identifier)));
}

TEST_F(SqliteReminderRepositoryTest, SchemaAndSqlFailuresAreNotMissingRecordsOrSuccessfulUpdates)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    auto reminder = examples().front();
    const auto identifier = repository.add(reminder);
    setIdentifier(reminder, identifier);
    EXPECT_THROW(database.execute(QStringLiteral("UPDATE reminders SET kind = 'Place'")), std::runtime_error);
    EXPECT_THROW(database.execute(QStringLiteral("UPDATE reminders SET latitude = 0")), std::runtime_error);
    database.execute(QStringLiteral("CREATE TRIGGER fail_reminder BEFORE UPDATE ON reminders "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.update(reminder)), std::runtime_error);
    expectReminder(*repository.find(identifier), reminder);
    database.execute(QStringLiteral("DROP TABLE reminders"));
    EXPECT_THROW(static_cast<void>(repository.find(identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.add(examples().front())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.update(reminder)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.remove(identifier)), std::runtime_error);
}

TEST_F(SqliteReminderRepositoryTest, OuterTransactionRollsBackAddVariantChangeAndRemove)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    auto original = examples().front();
    const auto identifier = repository.add(original);
    setIdentifier(original, identifier);
    EXPECT_THROW(database.transaction([&repository, identifier]() {
        core::Reminder replacement = core::PlaceReminder{.identifier = identifier, .title = "Замена", .place = "Место"};
        static_cast<void>(repository.update(replacement));
        static_cast<void>(repository.remove(identifier));
        static_cast<void>(repository.add(examples().back()));
        throw std::runtime_error{"откат напоминаний"};
    }), std::runtime_error);
    ASSERT_EQ(repository.all().size(), 1U);
    expectReminder(*repository.find(identifier), original);
    database.transaction([&repository, identifier]() { static_cast<void>(repository.remove(identifier)); });
    EXPECT_TRUE(repository.all().empty());
}

TEST_F(SqliteReminderRepositoryTest, ServicePersistsMissedFiredAndNextRecurringDueAcrossReopen)
{
    core::FakeClock clock{kMoment};
    std::int64_t missedIdentifier = 0;
    std::int64_t timedIdentifier = 0;
    std::int64_t recurringIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteReminderRepository repository{database};
        missedIdentifier = repository.add(core::TimedReminder{.dueAt = kMoment - std::chrono::seconds{1}});
        timedIdentifier = repository.add(core::TimedReminder{.dueAt = kMoment});
        recurringIdentifier = repository.add(core::RecurringReminder{.dueAt = kMoment - std::chrono::hours{5},
            .interval = std::chrono::minutes{15}});
        core::ReminderService service{clock, repository};
        service.skipMissedOnStartup();
        EXPECT_EQ(service.checkDue().size(), 2U);
        EXPECT_TRUE(service.checkDue().empty());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    EXPECT_EQ(std::get<core::TimedReminder>(*repository.find(missedIdentifier)).status, core::ReminderStatus::Missed);
    EXPECT_EQ(std::get<core::TimedReminder>(*repository.find(timedIdentifier)).status, core::ReminderStatus::Fired);
    EXPECT_EQ(std::get<core::RecurringReminder>(*repository.find(recurringIdentifier)).dueAt,
        kMoment + std::chrono::minutes{15});
    core::ReminderService service{clock, repository};
    EXPECT_TRUE(service.checkDue().empty());
    ASSERT_EQ(service.activeReminders().size(), 1U);
    clock.advance(std::chrono::minutes{15});
    EXPECT_EQ(service.checkDue().size(), 1U);
}

TEST_F(SqliteReminderRepositoryTest, ServicePreservesUnresolvedPlaceAndDoesNotFireResolvedPlaceTwice)
{
    core::FakeClock clock{kMoment};
    std::int64_t unresolvedIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteReminderRepository repository{database};
        unresolvedIdentifier = repository.add(core::PlaceReminder{.title = "Без координат", .place = "Место"});
        static_cast<void>(repository.add(core::PlaceReminder{.title = "Нули", .place = "Точка",
            .coordinates = core::GeoCoordinates{0.0, 0.0}}));
        core::ReminderService service{clock, repository};
        const auto result = service.checkNearPlace({0.0, 0.0});
        ASSERT_TRUE(std::holds_alternative<std::vector<core::PlaceReminder>>(result));
        EXPECT_EQ(std::get<std::vector<core::PlaceReminder>>(result).size(), 1U);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteReminderRepository repository{database};
    core::ReminderService service{clock, repository};
    const auto result = service.checkNearPlace({0.0, 0.0});
    ASSERT_TRUE(std::holds_alternative<std::vector<core::PlaceReminder>>(result));
    EXPECT_TRUE(std::get<std::vector<core::PlaceReminder>>(result).empty());
    const auto unresolved = std::get<core::PlaceReminder>(*repository.find(unresolvedIdentifier));
    EXPECT_FALSE(unresolved.coordinates);
    EXPECT_EQ(unresolved.status, core::ReminderStatus::Pending);
    ASSERT_EQ(service.activeReminders().size(), 1U);
    EXPECT_EQ(identifierOf(service.activeReminders().front()), unresolvedIdentifier);
}

}  // namespace
}  // namespace msa::infra

