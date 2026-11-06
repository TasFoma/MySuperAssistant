#include "msa/core/UserProfileFactSelector.h"
#include "msa/core/UserProfileService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteUserProfileRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::infra {
namespace {

constexpr int kFactCount = 35;

[[nodiscard]] core::UserProfile sampleProfile()
{
    // Порядок добавления не обязан совпадать с порядком идентификаторов.
    return {.facts = {
        {.text = "  Первый факт\n🙂 '); DELETE FROM chats; --  ",
            .tags = {"Работа", "Работа", "Два слова", "🙂"}, .alwaysInclude = true, .identifier = 9},
        {.text = "Второй факт", .identifier = 2}}};
}

[[nodiscard]] core::ProfileFactSource addSource(SqliteDatabase& database)
{
    SqliteChatRepository chats{database};
    const auto chatIdentifier = chats.addChat({.title = "Исходный чат"});
    const auto messageIdentifier = chats.addMessage({.chatIdentifier = chatIdentifier,
        .role = core::MessageRole::User, .text = "Полный исходный текст"});
    if (!messageIdentifier) {
        throw std::runtime_error{"Не создан тестовый источник"};
    }

    return {.chatIdentifier = chatIdentifier, .messageIdentifier = *messageIdentifier};
}

class SqliteUserProfileRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-profile-test-XXXXXX")};
};

void SqliteUserProfileRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteUserProfileRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("профиль.sqlite"));
}

TEST_F(SqliteUserProfileRepositoryTest, UpgradesVersionTwoWithoutChangingChatsAndStartsEmpty)
{
    auto migrations = sqliteMigrations();
    migrations.resize(1);
    std::int64_t identifier = 0;
    {
        SqliteDatabase database{path(), migrations};
        SqliteChatRepository chats{database};
        identifier = chats.addChat({.title = "Сохранённый чат"});
        ASSERT_TRUE(chats.saveCurrentChatId(identifier));
        ASSERT_TRUE(chats.addMessage({.chatIdentifier = identifier, .text = "Сохранённое сообщение"}));
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    EXPECT_EQ(database.schemaVersion(), sqliteMigrations().back().version);
    EXPECT_TRUE(repository.load().facts.empty());
    SqliteChatRepository chats{database};
    EXPECT_EQ(chats.currentChatId(), identifier);
    EXPECT_EQ(chats.findChat(identifier)->title, "Сохранённый чат");
    ASSERT_EQ(chats.messages(identifier).size(), 1U);
    EXPECT_EQ(chats.messages(identifier).front().text, "Сохранённое сообщение");
}

TEST_F(SqliteUserProfileRepositoryTest, ReopensWithExactIdsOrderTextsTagsAndPriority)
{
    auto profile = sampleProfile();
    profile.facts.front().text.append(20000, 'x');
    profile.facts.front().text.append("\0конец", sizeof("\0конец") - 1);
    profile.facts.front().tags.push_back(std::string{"тег\0конец", sizeof("тег\0конец") - 1});
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteUserProfileRepository repository{database};
        repository.save(profile);
        EXPECT_EQ(repository.load().facts, profile.facts);
        repository.save(profile);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    EXPECT_EQ(repository.load().facts, profile.facts);
    auto copy = repository.load();
    copy.facts.front().text.clear();
    copy.facts.front().tags.clear();
    EXPECT_EQ(repository.load().facts, profile.facts);
}

TEST_F(SqliteUserProfileRepositoryTest, KeepsSourcesNullableMoodAndBidirectionalVersionChain)
{
    core::UserProfile profile;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteUserProfileRepository repository{database};
        auto source = addSource(database);
        auto moodSource = source;
        moodSource.moodIdentifier = 123;
        profile.facts = {
            {.text = "Ручной факт", .identifier = 1},
            {.text = "Старая версия", .tags = {"тег"}, .alwaysInclude = true,
                .identifier = 2, .source = source, .supersededByFactIdentifier = 3},
            {.text = "Новая версия", .tags = {"тег"}, .alwaysInclude = true,
                .identifier = 3, .source = moodSource, .replacesFactIdentifier = 2}};
        repository.save(profile);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    EXPECT_EQ(repository.load().facts, profile.facts);
    SqliteChatRepository chats{database};
    const auto source = *profile.facts.back().source;
    ASSERT_EQ(chats.messages(source.chatIdentifier).size(), 1U);
    EXPECT_EQ(chats.messages(source.chatIdentifier).front().text, "Полный исходный текст");
}

TEST_F(SqliteUserProfileRepositoryTest, ReplacesWholeSnapshotIncludingEmptyProfileAndRepeatedTags)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    repository.save(sampleProfile());
    auto replacement = sampleProfile();
    replacement.facts.erase(replacement.facts.begin());
    replacement.facts.front().tags = {"новый", "новый"};
    repository.save(replacement);
    EXPECT_EQ(repository.load().facts, replacement.facts);
    repository.save({});
    EXPECT_TRUE(repository.load().facts.empty());
    repository.save(sampleProfile());
    EXPECT_EQ(repository.load().facts, sampleProfile().facts);
}

TEST_F(SqliteUserProfileRepositoryTest, RollsBackEntireSaveOnDuplicateIdOrLateTagFailure)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    const auto original = sampleProfile();
    repository.save(original);
    auto invalid = original;
    invalid.facts.push_back(invalid.facts.front());
    EXPECT_THROW(repository.save(invalid), std::runtime_error);
    EXPECT_EQ(repository.load().facts, original.facts);
    invalid = original;
    invalid.facts.front().identifier = 0;
    EXPECT_THROW(repository.save(invalid), std::runtime_error);
    EXPECT_EQ(repository.load().facts, original.facts);
    database.execute(QStringLiteral("CREATE TRIGGER fail_tag BEFORE INSERT ON profile_fact_tags "
        "WHEN NEW.text = 'ошибка' BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    auto replacement = original;
    replacement.facts.back().tags = {"первый тег", "ошибка"};
    EXPECT_THROW(repository.save(replacement), std::runtime_error);
    EXPECT_EQ(repository.load().facts, original.facts);
}

TEST_F(SqliteUserProfileRepositoryTest, RejectsMissingVersionAndMismatchedSourceWithoutLosingProfile)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    repository.save(sampleProfile());
    auto replacement = sampleProfile();
    replacement.facts.front().replacesFactIdentifier = 123;
    EXPECT_THROW(repository.save(replacement), std::runtime_error);
    EXPECT_EQ(repository.load().facts, sampleProfile().facts);
    replacement = sampleProfile();
    auto source = addSource(database);
    SqliteChatRepository chats{database};
    source.chatIdentifier = chats.addChat({.title = "Другой чат"});
    replacement.facts.front().source = source;
    EXPECT_THROW(repository.save(replacement), std::runtime_error);
    EXPECT_EQ(repository.load().facts, sampleProfile().facts);
}

TEST_F(SqliteUserProfileRepositoryTest, ParticipatesInOuterTransactionAndKeepsUserBeforeIt)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    const auto source = addSource(database);
    repository.save(sampleProfile());
    EXPECT_THROW(database.transaction([&database, &repository, source]() {
        auto updated = sampleProfile();
        updated.facts.back().source = source;
        repository.save(updated);
        SqliteChatRepository chats{database};
        static_cast<void>(chats.addMessage({.chatIdentifier = source.chatIdentifier,
            .role = core::MessageRole::Assistant, .text = "Ответ"}));
        throw std::runtime_error{"сбой после сохранения профиля"};
    }), std::runtime_error);
    EXPECT_EQ(repository.load().facts, sampleProfile().facts);
    SqliteChatRepository chats{database};
    EXPECT_EQ(chats.messages(source.chatIdentifier).size(), 1U);
    database.transaction([&repository]() { repository.save({}); });
    EXPECT_TRUE(repository.load().facts.empty());
}

TEST_F(SqliteUserProfileRepositoryTest, ServiceCreatesPersistentVersionsAndSkipsExactAutomaticRepeats)
{
    std::int64_t latestIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteUserProfileRepository repository{database};
        core::UserProfileService service{repository};
        const auto source = addSource(database);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Старый факт", {"тег"}, true)));
        const auto originalIdentifier = service.profile().facts.front().identifier;
        const auto result = service.rememberExtractedFacts(
            {{.text = "Новый факт", .replacesFactIdentifier = originalIdentifier}}, source);
        ASSERT_TRUE(std::holds_alternative<std::vector<core::UserProfileFact>>(result));
        latestIdentifier = service.profile().facts.back().identifier;
        const auto repeat = service.rememberExtractedFacts({{.text = "Новый факт"}}, source);
        ASSERT_TRUE(std::holds_alternative<std::vector<core::UserProfileFact>>(repeat));
        EXPECT_TRUE(std::get<std::vector<core::UserProfileFact>>(repeat).empty());
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    core::UserProfileService service{repository};
    const auto facts = service.profile().facts;
    ASSERT_EQ(facts.size(), 2U);
    EXPECT_EQ(facts.front().supersededByFactIdentifier, latestIdentifier);
    EXPECT_EQ(facts.back().replacesFactIdentifier, facts.front().identifier);
    EXPECT_EQ(facts.back().tags, facts.front().tags);
    EXPECT_TRUE(facts.back().alwaysInclude);
    const core::UserProfileFactSelector selector;
    const auto selected = selector.selectFacts(service.profile(), "");
    ASSERT_EQ(selected.size(), 1U);
    EXPECT_EQ(selected.front().identifier, latestIdentifier);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Ещё факт")));
    EXPECT_GT(service.profile().facts.back().identifier, latestIdentifier);
}

TEST_F(SqliteUserProfileRepositoryTest, KeepsMoreThanThirtyFactsIncludingManualRepeats)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    core::UserProfileService service{repository};
    for (int index = 0; index < kFactCount; ++index) {
        ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Повторяемый факт")));
    }
    const auto profile = repository.load();
    ASSERT_EQ(profile.facts.size(), static_cast<std::size_t>(kFactCount));
    EXPECT_EQ(profile.facts.front().identifier, 1);
    EXPECT_EQ(profile.facts.back().identifier, kFactCount);
}

TEST_F(SqliteUserProfileRepositoryTest, ReadErrorsPropagateInsteadOfReturningEmptyProfile)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteUserProfileRepository repository{database};
    repository.save(sampleProfile());
    database.execute(QStringLiteral("DROP TABLE profile_fact_tags"));
    EXPECT_THROW(static_cast<void>(repository.load()), std::runtime_error);
    EXPECT_THROW(repository.save(sampleProfile()), std::runtime_error);
}

}  // namespace
}  // namespace msa::infra

