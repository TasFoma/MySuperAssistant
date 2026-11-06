#include "../core/FakeClock.h"

#include "msa/core/MoodService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteMoodRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteUserProfileRepository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace msa::infra {
namespace {

constexpr auto kRecordedAt = std::chrono::local_seconds{std::chrono::seconds{-123456789}};

[[nodiscard]] core::ProfileFactSource createSource(SqliteDatabase& database)
{
    SqliteChatRepository chats{database};
    const auto chatIdentifier = chats.addChat({.title = "Дневник"});
    const auto messageIdentifier = chats.addMessage({.chatIdentifier = chatIdentifier, .text = "Исходник"});
    if (!messageIdentifier) {
        throw std::runtime_error{"Не создан тестовый источник"};
    }

    return {.chatIdentifier = chatIdentifier, .messageIdentifier = *messageIdentifier};
}

[[nodiscard]] core::UserProfile versionedProfile(const core::ProfileFactSource& source)
{
    return {.facts = {
        {.text = "Старая версия 🙂", .tags = {"тег", "тег"}, .alwaysInclude = true,
            .identifier = 9, .source = source, .supersededByFactIdentifier = 2},
        {.text = "Новая версия\nс подробностями", .tags = {"новый", "тег"}, .identifier = 2,
            .source = source, .replacesFactIdentifier = 9}}};
}

class SqliteMoodRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-mood-test-XXXXXX")};
};

void SqliteMoodRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteMoodRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("дневник.sqlite"));
}

TEST_F(SqliteMoodRepositoryTest, UpgradesVersionSixPreservingProfileVersionsTagsAndSources)
{
    auto migrations = sqliteMigrations();
    migrations.resize(5);
    core::UserProfile profile;
    {
        SqliteDatabase database{path(), migrations};
        profile = versionedProfile(createSource(database));
        SqliteUserProfileRepository profiles{database};
        profiles.save(profile);
    }
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        EXPECT_EQ(database.schemaVersion(), sqliteMigrations().back().version);
        SqliteUserProfileRepository profiles{database};
        EXPECT_EQ(profiles.load().facts, profile.facts);
        // После переименования таблиц должны работать и самоссылки, и ссылки тегов.
        profiles.save(profile);
        EXPECT_EQ(profiles.load().facts, profile.facts);
        SqliteMoodRepository moods{database};
        EXPECT_TRUE(moods.all().empty());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository profiles{database};
    EXPECT_EQ(profiles.load().facts, profile.facts);
}

TEST_F(SqliteMoodRepositoryTest, DanglingLegacyMoodSourceRejectsMigrationWithoutChangingFile)
{
    auto migrations = sqliteMigrations();
    migrations.resize(5);
    core::UserProfile profile;
    {
        SqliteDatabase database{path(), migrations};
        auto source = createSource(database);
        source.moodIdentifier = 123;
        profile = versionedProfile(source);
        SqliteUserProfileRepository profiles{database};
        profiles.save(profile);
    }
    EXPECT_THROW((SqliteDatabase{path(), sqliteMigrations()}), std::runtime_error);
    SqliteDatabase database{path(), migrations};
    EXPECT_EQ(database.schemaVersion(), 6);
    SqliteUserProfileRepository profiles{database};
    EXPECT_EQ(profiles.load().facts, profile.facts);
    QSqlQuery query{database.connection()};
    ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_schema "
        "WHERE name IN ('mood_entries', 'profile_facts_new', 'profile_fact_tags_new')")));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.value(0).toInt(), 0);
}

TEST_F(SqliteMoodRepositoryTest, ReopensWithFullNotesScoresTimesAndPersistentIdentifiers)
{
    std::string note = "  Полная заметка\n🙂 '); DELETE FROM mood_entries; --  ";
    note.append(20000, 'x');
    note.append("\0конец", sizeof("\0конец") - 1);
    std::int64_t lastIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteMoodRepository repository{database};
        for (const auto score : std::array{core::MoodScore::Negative, core::MoodScore::Neutral,
                 core::MoodScore::Positive}) {
            const core::MoodEntry entry{.recordedAt = kRecordedAt, .score = score, .note = note};
            const auto identifier = repository.add(entry);
            EXPECT_GT(identifier, lastIdentifier);
            lastIdentifier = identifier;
            EXPECT_EQ(entry.identifier, 0);
        }
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteMoodRepository repository{database};
    auto entries = repository.all();
    std::ranges::sort(entries, {}, &core::MoodEntry::identifier);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].score, core::MoodScore::Negative);
    EXPECT_EQ(entries[1].score, core::MoodScore::Neutral);
    EXPECT_EQ(entries[2].score, core::MoodScore::Positive);
    for (const auto& entry : entries) {
        EXPECT_EQ(entry.recordedAt, kRecordedAt);
        EXPECT_EQ(entry.note, note);
    }
    entries.front().note.clear();
    EXPECT_EQ(repository.all().front().note, note);
    EXPECT_GT(repository.add({}), lastIdentifier);
    EXPECT_GT(repository.add({}), lastIdentifier);
    EXPECT_EQ(repository.all().size(), 5U);
}

TEST_F(SqliteMoodRepositoryTest, RejectsInvalidInputAndPropagatesReadWriteErrors)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteMoodRepository repository{database};
    EXPECT_THROW(static_cast<void>(repository.add({.identifier = 1})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(repository.add({.score = static_cast<core::MoodScore>(9)})),
        std::invalid_argument);
    EXPECT_TRUE(repository.all().empty());
    database.execute(QStringLiteral("CREATE TRIGGER fail_mood BEFORE INSERT ON mood_entries "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(repository.add({})), std::runtime_error);
    EXPECT_TRUE(repository.all().empty());
    database.execute(QStringLiteral("DROP TABLE mood_entries"));
    EXPECT_THROW(static_cast<void>(repository.all()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.add({})), std::runtime_error);
}

TEST_F(SqliteMoodRepositoryTest, ProfileReferencesExistingMoodAndRejectsMissingMoodWithoutLosingFacts)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteMoodRepository moods{database};
    SqliteUserProfileRepository profiles{database};
    auto source = createSource(database);
    source.moodIdentifier = moods.add({.note = "Полный исходник"});
    const auto profile = versionedProfile(source);
    profiles.save(profile);
    EXPECT_EQ(profiles.load().facts, profile.facts);
    auto invalid = profile;
    invalid.facts.back().source->moodIdentifier = *source.moodIdentifier + 1;
    EXPECT_THROW(profiles.save(invalid), std::runtime_error);
    EXPECT_EQ(profiles.load().facts, profile.facts);
    EXPECT_THROW(database.execute(QStringLiteral("DELETE FROM mood_entries")), std::runtime_error);
    EXPECT_EQ(moods.all().size(), 1U);
}

TEST_F(SqliteMoodRepositoryTest, OuterTransactionRollsBackDiaryAndProfileButKeepsUser)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteMoodRepository moods{database};
    SqliteUserProfileRepository profiles{database};
    const auto source = createSource(database);
    EXPECT_THROW(database.transaction([&moods, &profiles, source]() {
        auto linked = source;
        linked.moodIdentifier = moods.add({.note = "Настроение"});
        profiles.save(versionedProfile(linked));
        throw std::runtime_error{"сбой общей операции"};
    }), std::runtime_error);
    EXPECT_TRUE(moods.all().empty());
    EXPECT_TRUE(profiles.load().facts.empty());
    SqliteChatRepository chats{database};
    EXPECT_EQ(chats.messages(source.chatIdentifier).size(), 1U);
    database.transaction([&moods, &profiles, source]() {
        auto linked = source;
        linked.moodIdentifier = moods.add({.note = "Настроение"});
        profiles.save(versionedProfile(linked));
    });
    EXPECT_EQ(moods.all().size(), 1U);
    EXPECT_EQ(profiles.load().facts.size(), 2U);
}

TEST_F(SqliteMoodRepositoryTest, ServiceFiltersHistoryWithoutDeletingOldOrFutureEntriesAfterReopen)
{
    using std::chrono::year;
    const auto today = std::chrono::local_days{year{2026} / 11 / 6};
    core::FakeClock clock{today + std::chrono::hours{12}};
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteMoodRepository repository{database};
        static_cast<void>(repository.add({.recordedAt = today - std::chrono::days{30}, .note = "Старая"}));
        static_cast<void>(repository.add({.recordedAt = today + std::chrono::days{1}, .note = "Будущая"}));
        core::MoodService service{clock, repository};
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(service.logMood(core::MoodScore::Positive, "Сегодня")));
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(service.logMood(core::MoodScore::Positive, "Сегодня")));
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteMoodRepository repository{database};
    core::MoodService service{clock, repository};
    const auto recent = service.moodHistory();
    ASSERT_TRUE(std::holds_alternative<std::vector<core::MoodEntry>>(recent));
    EXPECT_EQ(std::get<std::vector<core::MoodEntry>>(recent).size(), 2U);
    const auto extended = service.moodHistory(40);
    ASSERT_TRUE(std::holds_alternative<std::vector<core::MoodEntry>>(extended));
    const auto& entries = std::get<std::vector<core::MoodEntry>>(extended);
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries.front().note, "Старая");
    EXPECT_LT(entries[1].identifier, entries[2].identifier);
    EXPECT_EQ(repository.all().size(), 4U);
}

}  // namespace
}  // namespace msa::infra

