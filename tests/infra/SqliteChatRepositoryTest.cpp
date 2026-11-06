#include "msa/core/ChatService.h"
#include "msa/core/Clock.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteSchema.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace msa::infra {
namespace {

constexpr auto kCreatedAt = std::chrono::local_seconds{std::chrono::seconds{-123456789}};
constexpr std::int64_t kMissingIdentifier = 999999;
constexpr int kMessageCount = 45;

class FixedClock final : public core::Clock {
public:
    [[nodiscard]] std::chrono::local_seconds now() const override;
};

std::chrono::local_seconds FixedClock::now() const
{
    return std::chrono::local_seconds{std::chrono::days{20000}};
}

[[nodiscard]] core::Chat sampleChat()
{
    return {.title = "  Личный чат 🙂 '); DROP TABLE chats; --\n", .createdAt = kCreatedAt};
}

[[nodiscard]] core::ChatMessage sampleMessage(std::int64_t chatIdentifier)
{
    return {.chatIdentifier = chatIdentifier, .role = core::MessageRole::User,
        .text = "  Полный текст\nс переносом, е́ и 🙂 '); DELETE FROM chats; --  ",
        .createdAt = kCreatedAt};
}

class SqliteChatRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-chat-test-XXXXXX")};
};

void SqliteChatRepositoryTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteChatRepositoryTest::path() const
{
    return m_directory.filePath(QStringLiteral("чаты.sqlite"));
}

TEST_F(SqliteChatRepositoryTest, MigratesBaselineAndReopensWithoutLosingChats)
{
    {
        const SqliteDatabase baseline{path()};
        EXPECT_EQ(baseline.schemaVersion(), 1);
    }
    std::int64_t identifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteChatRepository repository{database};
        EXPECT_EQ(database.schemaVersion(), 2);
        EXPECT_TRUE(repository.chats().empty());
        EXPECT_FALSE(repository.currentChatId());
        identifier = repository.addChat(sampleChat());
        ASSERT_GT(identifier, 0);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    ASSERT_EQ(repository.chats().size(), 1U);
    const auto chat = repository.findChat(identifier);
    ASSERT_TRUE(chat);
    EXPECT_EQ(chat->title, sampleChat().title);
    EXPECT_EQ(chat->createdAt, kCreatedAt);
    EXPECT_GT(repository.addChat(sampleChat()), identifier);
}

TEST_F(SqliteChatRepositoryTest, PersistsSelectionAndRejectsMissingChatWithoutChangingIt)
{
    std::int64_t firstIdentifier = 0;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteChatRepository repository{database};
        EXPECT_FALSE(repository.saveCurrentChatId(kMissingIdentifier));
        EXPECT_FALSE(repository.currentChatId());
        firstIdentifier = repository.addChat(sampleChat());
        const auto secondIdentifier = repository.addChat(sampleChat());
        ASSERT_TRUE(repository.saveCurrentChatId(secondIdentifier));
        EXPECT_EQ(repository.currentChatId(), secondIdentifier);
        ASSERT_TRUE(repository.saveCurrentChatId(firstIdentifier));
        EXPECT_TRUE(repository.saveCurrentChatId(firstIdentifier));
        EXPECT_FALSE(repository.saveCurrentChatId(kMissingIdentifier));
        EXPECT_EQ(repository.currentChatId(), firstIdentifier);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    EXPECT_EQ(repository.currentChatId(), firstIdentifier);
}

TEST_F(SqliteChatRepositoryTest, MissingChatHasNoMessagesAndCannotReceiveMessage)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    EXPECT_FALSE(repository.findChat(kMissingIdentifier));
    EXPECT_TRUE(repository.messages(kMissingIdentifier).empty());
    EXPECT_FALSE(repository.addMessage(sampleMessage(kMissingIdentifier)));
    EXPECT_TRUE(repository.chats().empty());
}

TEST_F(SqliteChatRepositoryTest, PreservesLongMessagesRolesTimesAndSeparateHistoriesAcrossReopen)
{
    std::int64_t chatIdentifier = 0;
    std::int64_t otherIdentifier = 0;
    std::int64_t userIdentifier = 0;
    auto original = sampleMessage(0);
    original.text.append(20000, 'x');
    original.text.append("\0после нуля", sizeof("\0после нуля") - 1);
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteChatRepository repository{database};
        chatIdentifier = repository.addChat(sampleChat());
        otherIdentifier = repository.addChat(sampleChat());
        original.chatIdentifier = chatIdentifier;
        const auto saved = repository.addMessage(original);
        ASSERT_TRUE(saved);
        userIdentifier = *saved;
        auto reply = sampleMessage(chatIdentifier);
        reply.role = core::MessageRole::Assistant;
        reply.createdAt -= std::chrono::hours{1};
        const auto replyIdentifier = repository.addMessage(reply);
        ASSERT_TRUE(replyIdentifier);
        EXPECT_GT(*replyIdentifier, userIdentifier);
        ASSERT_TRUE(repository.addMessage(sampleMessage(otherIdentifier)));
        EXPECT_EQ(original.identifier, 0);
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    auto messages = repository.messages(chatIdentifier);
    std::ranges::sort(messages, {}, &core::ChatMessage::identifier);
    ASSERT_EQ(messages.size(), 2U);
    EXPECT_EQ(messages[0].identifier, userIdentifier);
    EXPECT_EQ(messages[0].chatIdentifier, chatIdentifier);
    EXPECT_EQ(messages[0].text, original.text);
    EXPECT_EQ(messages[0].createdAt, original.createdAt);
    EXPECT_EQ(messages[0].role, core::MessageRole::User);
    EXPECT_EQ(messages[1].role, core::MessageRole::Assistant);
    EXPECT_EQ(messages[1].createdAt, kCreatedAt - std::chrono::hours{1});
    EXPECT_EQ(repository.messages(otherIdentifier).size(), 1U);
    const auto newIdentifier = repository.addMessage(sampleMessage(chatIdentifier));
    ASSERT_TRUE(newIdentifier);
    EXPECT_GT(*newIdentifier, messages[1].identifier);
}

TEST_F(SqliteChatRepositoryTest, ReturnsIndependentCopiesAndKeepsRepeatedMessagesWithoutLimit)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    const auto chat = sampleChat();
    const auto identifier = repository.addChat(chat);
    EXPECT_EQ(chat.identifier, 0);
    const auto message = sampleMessage(identifier);
    for (int index = 0; index < kMessageCount; ++index) {
        ASSERT_TRUE(repository.addMessage(message));
    }
    auto chats = repository.chats();
    chats.front().title = "изменено";
    auto found = repository.findChat(identifier);
    ASSERT_TRUE(found);
    found->title.clear();
    auto messages = repository.messages(identifier);
    ASSERT_EQ(messages.size(), static_cast<std::size_t>(kMessageCount));
    messages.front().text.clear();
    EXPECT_EQ(repository.findChat(identifier)->title, chat.title);
    EXPECT_EQ(repository.messages(identifier).front().text, message.text);
}

TEST_F(SqliteChatRepositoryTest, RejectsNonzeroIdentifiersAndUnknownRoleWithoutWrites)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    auto chat = sampleChat();
    chat.identifier = 1;
    EXPECT_THROW(static_cast<void>(repository.addChat(chat)), std::invalid_argument);
    EXPECT_TRUE(repository.chats().empty());
    const auto identifier = repository.addChat(sampleChat());
    auto message = sampleMessage(identifier);
    message.identifier = 1;
    EXPECT_THROW(static_cast<void>(repository.addMessage(message)), std::invalid_argument);
    message.identifier = 0;
    message.role = static_cast<core::MessageRole>(255);
    EXPECT_THROW(static_cast<void>(repository.addMessage(message)), std::invalid_argument);
    EXPECT_TRUE(repository.messages(identifier).empty());
}

TEST_F(SqliteChatRepositoryTest, SchemaEnforcesForeignKeysRoleAndSingleSelection)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO chat_messages "
        "(chat_identifier, role, text, created_at) VALUES (999, 'User', 'текст', 0)")),
        std::runtime_error);
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO chat_selection VALUES (1, 999)")),
        std::runtime_error);
    SqliteChatRepository repository{database};
    ASSERT_GT(repository.addChat(sampleChat()), 0);
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO chat_messages "
        "(chat_identifier, role, text, created_at) VALUES (1, 'System', 'текст', 0)")),
        std::runtime_error);
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO chat_selection VALUES (2, 1)")),
        std::runtime_error);
}

TEST_F(SqliteChatRepositoryTest, RepositoryWritesParticipateInOuterTransaction)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    const auto originalIdentifier = repository.addChat(sampleChat());
    ASSERT_TRUE(repository.saveCurrentChatId(originalIdentifier));
    EXPECT_THROW(database.transaction([&repository]() {
        const auto identifier = repository.addChat(sampleChat());
        static_cast<void>(repository.addMessage(sampleMessage(identifier)));
        static_cast<void>(repository.saveCurrentChatId(identifier));
        throw std::runtime_error{"сбой общей операции"};
    }), std::runtime_error);
    EXPECT_EQ(repository.chats().size(), 1U);
    EXPECT_EQ(repository.currentChatId(), originalIdentifier);
    database.transaction([&repository, originalIdentifier]() {
        static_cast<void>(repository.addMessage(sampleMessage(originalIdentifier)));
    });
    EXPECT_EQ(repository.messages(originalIdentifier).size(), 1U);
}

TEST_F(SqliteChatRepositoryTest, PropagatesSqlErrorsAndPreservesPreviousSelectionOnWriteFailure)
{
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    const auto identifier = repository.addChat(sampleChat());
    const auto otherIdentifier = repository.addChat(sampleChat());
    ASSERT_TRUE(repository.saveCurrentChatId(identifier));
    database.execute(QStringLiteral("CREATE TRIGGER fail_selection BEFORE UPDATE ON chat_selection "
                                    "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(repository.saveCurrentChatId(otherIdentifier), std::runtime_error);
    EXPECT_EQ(repository.currentChatId(), identifier);
    database.execute(QStringLiteral("DROP TABLE chat_messages"));
    EXPECT_THROW(static_cast<void>(repository.messages(identifier)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(repository.addMessage(sampleMessage(identifier))), std::runtime_error);
}

TEST_F(SqliteChatRepositoryTest, ChatServiceRestoresPersistedSelectionAndFullHistory)
{
    const FixedClock clock;
    std::int64_t selectedIdentifier = 0;
    const auto text = sampleMessage(0).text;
    {
        SqliteDatabase database{path(), sqliteMigrations()};
        SqliteChatRepository repository{database};
        core::ChatService service{clock, repository};
        selectedIdentifier = service.currentChatId();
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(
            service.addMessage(text, core::MessageRole::User)));
        EXPECT_GT(service.newChat(), selectedIdentifier);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(service.switchChat(selectedIdentifier)));
    }
    SqliteDatabase database{path(), sqliteMigrations()};
    SqliteChatRepository repository{database};
    core::ChatService service{clock, repository};
    EXPECT_EQ(service.currentChatId(), selectedIdentifier);
    const auto chats = service.chatList();
    ASSERT_EQ(chats.size(), 2U);
    EXPECT_GT(chats.front().identifier, chats.back().identifier);
    const auto messages = service.loadMessages();
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages.front().text, text);
    EXPECT_EQ(messages.front().createdAt, clock.now());
}

}  // namespace
}  // namespace msa::infra

