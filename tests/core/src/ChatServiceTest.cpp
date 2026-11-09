#include "FakeClock.h"
#include "InMemoryChatRepository.h"

#include "msa/core/ChatService.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::year;

constexpr std::int64_t kMissingChatId = 999;

enum class ChatFault {
    None,
    LoadChoice,
    Find,
    ReadChats,
    Create,
    SaveChoice,
    RejectChoice,
    StaleChoice,
    ReadMessages,
    WriteMessage,
    MissingMessageChat,
};

/// @brief Обёртка с управляемыми сбоями и изменённым порядком списков хранилища.
class ControlledChatRepository final : public ChatRepository {
public:
    explicit ControlledChatRepository(InMemoryChatRepository& storage);
    /// @brief Меняет вид сбоя, не изменяя сохранённых данных.
    void setFault(ChatFault fault);
    [[nodiscard]] std::int64_t addChat(const Chat& chat) override;
    [[nodiscard]] std::optional<Chat> findChat(std::int64_t identifier) const override;
    [[nodiscard]] std::vector<Chat> chats() const override;
    [[nodiscard]] std::optional<std::int64_t> addMessage(const ChatMessage& message) override;
    [[nodiscard]] std::vector<ChatMessage> messages(std::int64_t chatIdentifier) const override;
    [[nodiscard]] std::optional<std::int64_t> currentChatId() const override;
    bool saveCurrentChatId(std::int64_t identifier) override;

private:
    InMemoryChatRepository& m_storage;
    ChatFault m_fault{ChatFault::None};
};

ControlledChatRepository::ControlledChatRepository(InMemoryChatRepository& storage) : m_storage{storage}
{
}

void ControlledChatRepository::setFault(ChatFault fault)
{
    m_fault = fault;
}

std::int64_t ControlledChatRepository::addChat(const Chat& chat)
{
    if (m_fault == ChatFault::Create) {
        throw std::runtime_error{"Ошибка создания чата"};
    }

    return m_storage.addChat(chat);
}

std::optional<Chat> ControlledChatRepository::findChat(std::int64_t identifier) const
{
    if (m_fault == ChatFault::Find) {
        throw std::runtime_error{"Ошибка поиска чата"};
    }

    return m_storage.findChat(identifier);
}

std::vector<Chat> ControlledChatRepository::chats() const
{
    if (m_fault == ChatFault::ReadChats) {
        throw std::runtime_error{"Ошибка чтения списка чатов"};
    }
    auto chats = m_storage.chats();
    if (chats.size() > 1U) {
        std::ranges::rotate(chats, chats.begin() + 1);
    }

    return chats;
}

std::optional<std::int64_t> ControlledChatRepository::addMessage(const ChatMessage& message)
{
    if (m_fault == ChatFault::WriteMessage) {
        throw std::runtime_error{"Ошибка сохранения сообщения"};
    }
    if (m_fault == ChatFault::MissingMessageChat) {
        return std::nullopt;
    }

    return m_storage.addMessage(message);
}

std::vector<ChatMessage> ControlledChatRepository::messages(std::int64_t chatIdentifier) const
{
    if (m_fault == ChatFault::ReadMessages) {
        throw std::runtime_error{"Ошибка чтения сообщений"};
    }
    auto messages = m_storage.messages(chatIdentifier);
    std::ranges::reverse(messages);

    return messages;
}

std::optional<std::int64_t> ControlledChatRepository::currentChatId() const
{
    if (m_fault == ChatFault::LoadChoice) {
        throw std::runtime_error{"Ошибка чтения текущего выбора"};
    }
    if (m_fault == ChatFault::StaleChoice) {
        return kMissingChatId;
    }

    return m_storage.currentChatId();
}

bool ControlledChatRepository::saveCurrentChatId(std::int64_t identifier)
{
    if (m_fault == ChatFault::SaveChoice) {
        throw std::runtime_error{"Ошибка сохранения выбора"};
    }
    if (m_fault == ChatFault::RejectChoice) {
        return false;
    }

    return m_storage.saveCurrentChatId(identifier);
}

class ChatServiceTest : public ::testing::Test {
protected:
    FakeClock m_clock{local_days{year{2026} / 11 / 6} + hours{9} + minutes{5} + seconds{42}};
    InMemoryChatRepository m_storage{};
    ControlledChatRepository m_repository{m_storage};
};

TEST_F(ChatServiceTest, EmptyStartupCreatesDatedChatAndRecreationRestoresIt)
{
    ChatService service{m_clock, m_repository};
    const auto identifier = service.currentChatId();
    EXPECT_GT(identifier, 0);
    EXPECT_EQ(m_storage.currentChatId(), identifier);
    const auto chat = m_storage.findChat(identifier);
    ASSERT_TRUE(chat);
    EXPECT_EQ(chat->title, "Чат от 06.11.2026 09:05");
    EXPECT_EQ(chat->createdAt, m_clock.now());
    EXPECT_TRUE(service.loadMessages().empty());
    m_clock.advance(hours{1});
    ChatService recreated{m_clock, m_repository};
    EXPECT_EQ(recreated.currentChatId(), identifier);
    EXPECT_EQ(m_storage.chats().size(), 1U);
}

TEST_F(ChatServiceTest, StartupPrefersSavedChatOverLatest)
{
    const auto saved = m_storage.addChat(Chat{.title = "Выбранный"});
    static_cast<void>(m_storage.addChat(Chat{.title = "Последний"}));
    ASSERT_TRUE(m_storage.saveCurrentChatId(saved));
    ChatService service{m_clock, m_repository};
    EXPECT_EQ(service.currentChatId(), saved);
    EXPECT_EQ(m_storage.currentChatId(), saved);
    EXPECT_EQ(m_storage.chats().size(), 2U);
}

TEST_F(ChatServiceTest, MissingOrStaleSavedChoiceFallsBackToMaximumIdentifier)
{
    static_cast<void>(m_storage.addChat(Chat{.createdAt = m_clock.now()}));
    static_cast<void>(m_storage.addChat(Chat{.createdAt = m_clock.now() - hours{1}}));
    const auto latest = m_storage.addChat(Chat{.createdAt = m_clock.now() - hours{2}});
    ChatService service{m_clock, m_repository};
    EXPECT_EQ(service.currentChatId(), latest);
    EXPECT_EQ(m_storage.currentChatId(), latest);
    m_repository.setFault(ChatFault::StaleChoice);
    ChatService recreated{m_clock, m_repository};
    EXPECT_EQ(recreated.currentChatId(), latest);
    EXPECT_EQ(m_storage.currentChatId(), latest);
    EXPECT_EQ(m_storage.chats().size(), 3U);
}

TEST_F(ChatServiceTest, NewChatSelectsAndListsByDescendingIdentifierWithoutLosingEarlierData)
{
    ChatService service{m_clock, m_repository};
    const auto first = service.currentChatId();
    static_cast<void>(service.addMessage("Ранняя история", MessageRole::User));
    // Откат часов не меняет порядок создания по ID.
    m_clock.set(local_days{year{2027} / 1 / 1});
    const auto second = service.newChat();
    EXPECT_EQ(m_storage.findChat(second)->title, "Чат от 01.01.2027 00:00");
    m_clock.set(local_days{year{2026} / 12 / 31} + hours{23} + minutes{59});
    const auto third = service.newChat();
    auto chats = service.chatList();
    ASSERT_EQ(chats.size(), 3U);
    EXPECT_EQ(chats[0].identifier, third);
    EXPECT_EQ(chats[1].identifier, second);
    EXPECT_EQ(chats[2].identifier, first);
    EXPECT_EQ(service.currentChatId(), third);
    EXPECT_EQ(m_storage.currentChatId(), third);
    EXPECT_EQ(m_storage.messages(first).size(), 1U);
    chats.back().title.clear();
    EXPECT_FALSE(m_storage.findChat(first)->title.empty());
}

TEST_F(ChatServiceTest, SwitchSeparatesHistoriesAndRejectsMissingChatWithoutChangingChoice)
{
    ChatService service{m_clock, m_repository};
    const auto first = service.currentChatId();
    static_cast<void>(service.addMessage("Первый", MessageRole::User));
    const auto second = service.newChat();
    EXPECT_TRUE(service.loadMessages().empty());
    static_cast<void>(service.addMessage("Второй", MessageRole::Assistant));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.switchChat(first)));
    ASSERT_EQ(service.loadMessages().size(), 1U);
    EXPECT_EQ(service.loadMessages().front().text, "Первый");
    EXPECT_EQ(m_storage.currentChatId(), first);
    for (const auto identifier : {std::int64_t{-1}, std::int64_t{0}, kMissingChatId}) {
        const auto result = service.switchChat(identifier);
        ASSERT_TRUE(std::holds_alternative<ChatRejection>(result));
        EXPECT_FALSE(std::get<ChatRejection>(result).reason.empty());
        EXPECT_EQ(service.currentChatId(), first);
        EXPECT_EQ(m_storage.currentChatId(), first);
    }
    EXPECT_EQ(m_storage.messages(second).front().text, "Второй");
    ChatService recreated{m_clock, m_repository};
    EXPECT_EQ(recreated.currentChatId(), first);
}

TEST_F(ChatServiceTest, StoresFullMessagesForBothRolesInInsertionOrderDespiteClockRollback)
{
    ChatService service{m_clock, m_repository};
    const auto startedAt = m_clock.now();
    const std::string longText = "  Важная история\n" + std::string(5000, 'x') + "🙂  ";
    const auto first = std::get<std::int64_t>(service.addMessage(longText, MessageRole::User));
    m_clock.advance(-seconds{1});
    const auto second = std::get<std::int64_t>(service.addMessage(" Ответ\n", MessageRole::Assistant));
    auto messages = service.loadMessages();
    ASSERT_EQ(messages.size(), 2U);
    EXPECT_EQ(messages[0].identifier, first);
    EXPECT_EQ(messages[0].chatIdentifier, service.currentChatId());
    EXPECT_EQ(messages[0].role, MessageRole::User);
    EXPECT_EQ(messages[0].text, longText);
    EXPECT_EQ(messages[0].createdAt, startedAt);
    EXPECT_EQ(messages[1].identifier, second);
    EXPECT_EQ(messages[1].role, MessageRole::Assistant);
    EXPECT_EQ(messages[1].text, " Ответ\n");
    EXPECT_EQ(messages[1].createdAt, m_clock.now());
    messages.front().text.clear();
    EXPECT_EQ(service.loadMessages().front().text, longText);
    constexpr int kExtraMessages = 45;
    for (int index = 0; index < kExtraMessages; ++index) {
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(service.addMessage("Повтор", MessageRole::User)));
    }
    EXPECT_EQ(service.loadMessages().size(), 2U + kExtraMessages);
    EXPECT_EQ(service.loadMessages().front().text, longText);
}

TEST_F(ChatServiceTest, RejectsBlankTextAndInvalidRoleBeforeWriting)
{
    ChatService service{m_clock, m_repository};
    m_repository.setFault(ChatFault::WriteMessage);
    for (const auto& text : {std::string{}, std::string{" \t\r\n"}, std::string{"\u00a0\u2003"}}) {
        const auto result = service.addMessage(text, MessageRole::User);
        ASSERT_TRUE(std::holds_alternative<ChatRejection>(result));
        EXPECT_FALSE(std::get<ChatRejection>(result).reason.empty());
    }
    const auto invalidRole = service.addMessage("Текст", static_cast<MessageRole>(255));
    ASSERT_TRUE(std::holds_alternative<ChatRejection>(invalidRole));
    EXPECT_FALSE(std::get<ChatRejection>(invalidRole).reason.empty());
    EXPECT_TRUE(m_storage.messages(service.currentChatId()).empty());
}

TEST_F(ChatServiceTest, StartupPropagatesReadAndWriteFailures)
{
    for (const auto fault : {ChatFault::LoadChoice, ChatFault::Find, ChatFault::ReadChats,
                             ChatFault::Create, ChatFault::SaveChoice, ChatFault::RejectChoice}) {
        InMemoryChatRepository storage;
        ControlledChatRepository repository{storage};
        if (fault == ChatFault::Find) {
            ASSERT_TRUE(storage.saveCurrentChatId(storage.addChat(Chat{})));
        }
        repository.setFault(fault);
        EXPECT_THROW(static_cast<void>(ChatService(m_clock, repository)), std::runtime_error);
    }
}

TEST_F(ChatServiceTest, FailedNewChatAndSwitchKeepPreviousChoice)
{
    ChatService service{m_clock, m_repository};
    const auto first = service.currentChatId();
    const auto second = service.newChat();
    m_repository.setFault(ChatFault::Create);
    EXPECT_THROW(static_cast<void>(service.newChat()), std::runtime_error);
    EXPECT_EQ(m_storage.chats().size(), 2U);
    for (const auto fault : {ChatFault::SaveChoice, ChatFault::RejectChoice}) {
        m_repository.setFault(fault);
        if (fault == ChatFault::SaveChoice) {
            EXPECT_THROW(static_cast<void>(service.switchChat(first)), std::runtime_error);
        } else {
            EXPECT_TRUE(std::holds_alternative<ChatRejection>(service.switchChat(first)));
        }
        const auto count = m_storage.chats().size();
        EXPECT_THROW(static_cast<void>(service.newChat()), std::runtime_error);
        EXPECT_EQ(m_storage.chats().size(), count + 1U);
        EXPECT_EQ(service.currentChatId(), second);
        EXPECT_EQ(m_storage.currentChatId(), second);
    }
}

TEST_F(ChatServiceTest, MessageAndListFailuresPropagateWithoutChangingHistory)
{
    ChatService service{m_clock, m_repository};
    static_cast<void>(service.addMessage("Сохранено", MessageRole::User));
    m_repository.setFault(ChatFault::WriteMessage);
    EXPECT_THROW(static_cast<void>(service.addMessage("Новое", MessageRole::Assistant)), std::runtime_error);
    m_repository.setFault(ChatFault::MissingMessageChat);
    EXPECT_TRUE(std::holds_alternative<ChatRejection>(service.addMessage("Новое", MessageRole::Assistant)));
    m_repository.setFault(ChatFault::ReadMessages);
    EXPECT_THROW(static_cast<void>(service.loadMessages()), std::runtime_error);
    m_repository.setFault(ChatFault::ReadChats);
    EXPECT_THROW(static_cast<void>(service.chatList()), std::runtime_error);
    const auto messages = m_storage.messages(service.currentChatId());
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages.front().text, "Сохранено");
}

TEST_F(ChatServiceTest, EmptyHistoryAcceptsZeroAndMaximumBudget)
{
    ChatService service{m_clock, m_repository};
    for (const auto budget : {std::size_t{0}, std::numeric_limits<std::size_t>::max()}) {
        const auto result = service.historyMessages(budget);
        ASSERT_TRUE(std::holds_alternative<std::vector<ChatMessage>>(result));
        EXPECT_TRUE(std::get<std::vector<ChatMessage>>(result).empty());
    }
}

TEST_F(ChatServiceTest, HistorySelectsContinuousNewestSuffixWithoutSkippingOversizedMiddle)
{
    ChatService service{m_clock, m_repository};
    static_cast<void>(service.addMessage("а", MessageRole::User));
    const auto middle = std::get<std::int64_t>(service.addMessage("12345678", MessageRole::Assistant));
    const auto recent = std::get<std::int64_t>(service.addMessage("Да", MessageRole::User));
    m_clock.advance(-hours{1});
    const auto latest = std::get<std::int64_t>(service.addMessage("🙂", MessageRole::Assistant));

    const auto limited = std::get<std::vector<ChatMessage>>(service.historyMessages(4));
    ASSERT_EQ(limited.size(), 2U);
    EXPECT_EQ(limited[0].identifier, recent);
    EXPECT_EQ(limited[0].text, "Да");
    EXPECT_EQ(limited[1].identifier, latest);
    EXPECT_EQ(limited[1].role, MessageRole::Assistant);
    EXPECT_EQ(limited[1].createdAt, m_clock.now());
    const auto exact = std::get<std::vector<ChatMessage>>(service.historyMessages(11));
    ASSERT_EQ(exact.size(), 3U);
    EXPECT_EQ(exact.front().identifier, middle);
    EXPECT_EQ(std::get<std::vector<ChatMessage>>(service.historyMessages(12)).size(), 4U);
    EXPECT_EQ(service.loadMessages().size(), 4U);
}

TEST_F(ChatServiceTest, HistoryCountsUnicodeCodePointsAndRejectsTooSmallBudgetWithoutTruncating)
{
    ChatService service{m_clock, m_repository};
    static_cast<void>(service.addMessage("е\u0301", MessageRole::User));
    static_cast<void>(service.addMessage("Я🙂", MessageRole::Assistant));
    for (const auto budget : {std::size_t{0}, std::size_t{1}}) {
        const auto result = service.historyMessages(budget);
        ASSERT_TRUE(std::holds_alternative<ChatRejection>(result));
        EXPECT_FALSE(std::get<ChatRejection>(result).reason.empty());
    }
    const auto exact = std::get<std::vector<ChatMessage>>(service.historyMessages(2));
    ASSERT_EQ(exact.size(), 1U);
    EXPECT_EQ(exact.front().text, "Я🙂");
    const auto spare = std::get<std::vector<ChatMessage>>(service.historyMessages(3));
    ASSERT_EQ(spare.size(), 1U);
    EXPECT_EQ(spare.front().text, "Я🙂");
    const auto all = std::get<std::vector<ChatMessage>>(service.historyMessages(4));
    ASSERT_EQ(all.size(), 2U);
    EXPECT_EQ(all.front().text, "е\u0301");
    EXPECT_EQ(service.loadMessages().back().text, "Я🙂");
}

TEST_F(ChatServiceTest, HistoryReadsOnlyCurrentChatAndReturnsCopies)
{
    ChatService service{m_clock, m_repository};
    const auto first = service.currentChatId();
    static_cast<void>(service.addMessage("Первый чат", MessageRole::User));
    const auto second = service.newChat();
    static_cast<void>(service.addMessage("Второй чат", MessageRole::Assistant));
    auto selected = std::get<std::vector<ChatMessage>>(service.historyMessages(100));
    ASSERT_EQ(selected.size(), 1U);
    EXPECT_EQ(selected.front().chatIdentifier, second);
    EXPECT_EQ(selected.front().text, "Второй чат");
    selected.front().text.clear();
    EXPECT_EQ(service.loadMessages().front().text, "Второй чат");
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.switchChat(first)));
    const auto previous = std::get<std::vector<ChatMessage>>(service.historyMessages(100));
    ASSERT_EQ(previous.size(), 1U);
    EXPECT_EQ(previous.front().text, "Первый чат");
    EXPECT_EQ(m_storage.messages(second).front().text, "Второй чат");
}

TEST_F(ChatServiceTest, HistoryHasNoSeparateFortyMessageOrFourThousandCharacterLimit)
{
    ChatService service{m_clock, m_repository};
    const std::string longText(5000, 'x');
    static_cast<void>(service.addMessage(longText, MessageRole::User));
    constexpr int kExtraMessages = 45;
    for (int index = 0; index < kExtraMessages; ++index) {
        static_cast<void>(service.addMessage("Я", MessageRole::Assistant));
    }
    const auto shortHistory = std::get<std::vector<ChatMessage>>(service.historyMessages(kExtraMessages));
    EXPECT_EQ(shortHistory.size(), kExtraMessages);
    const auto full = std::get<std::vector<ChatMessage>>(
        service.historyMessages(std::numeric_limits<std::size_t>::max()));
    ASSERT_EQ(full.size(), 1U + kExtraMessages);
    EXPECT_EQ(full.front().text, longText);
    EXPECT_EQ(service.loadMessages().front().text, longText);
}

TEST_F(ChatServiceTest, HistoryPropagatesReadFailureEvenWithZeroBudget)
{
    ChatService service{m_clock, m_repository};
    static_cast<void>(service.addMessage("Сохранено", MessageRole::User));
    m_repository.setFault(ChatFault::ReadMessages);
    EXPECT_THROW(static_cast<void>(service.historyMessages(100)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(service.historyMessages(0)), std::runtime_error);
    EXPECT_EQ(m_storage.messages(service.currentChatId()).front().text, "Сохранено");
}

TEST(ChatRepositoryTest, KeepsCopiesAndForeignKeysAndSelectionWithoutDeletingData)
{
    InMemoryChatRepository memory;
    ChatRepository& repository = memory;
    EXPECT_TRUE(repository.chats().empty());
    EXPECT_FALSE(repository.currentChatId());
    EXPECT_FALSE(repository.findChat(kMissingChatId));
    EXPECT_TRUE(repository.messages(kMissingChatId).empty());
    EXPECT_FALSE(repository.saveCurrentChatId(kMissingChatId));
    EXPECT_FALSE(repository.addMessage(ChatMessage{.chatIdentifier = kMissingChatId}));
    Chat chat{.title = "Чат"};
    const auto first = repository.addChat(chat);
    const auto second = repository.addChat(chat);
    EXPECT_GT(first, 0);
    EXPECT_GT(second, first);
    EXPECT_EQ(chat.identifier, 0);
    chat.title.clear();
    auto found = repository.findChat(first);
    ASSERT_TRUE(found);
    EXPECT_EQ(found->title, "Чат");
    EXPECT_THROW(static_cast<void>(repository.addChat(*found)), std::invalid_argument);
    found->title.clear();
    auto chats = repository.chats();
    chats.front().title.clear();
    EXPECT_EQ(repository.findChat(first)->title, "Чат");
    ASSERT_TRUE(repository.saveCurrentChatId(first));
    EXPECT_FALSE(repository.saveCurrentChatId(kMissingChatId));
    EXPECT_EQ(repository.currentChatId(), first);
    ChatMessage input{.chatIdentifier = first, .text = "Текст"};
    const auto messageId = repository.addMessage(input);
    ASSERT_TRUE(messageId);
    EXPECT_GT(*messageId, 0);
    EXPECT_EQ(input.identifier, 0);
    input.text.clear();
    auto messages = repository.messages(first);
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages.front().text, "Текст");
    EXPECT_THROW(static_cast<void>(repository.addMessage(messages.front())), std::invalid_argument);
    messages.front().text.clear();
    EXPECT_EQ(repository.messages(first).front().text, "Текст");
    EXPECT_TRUE(repository.messages(second).empty());
    EXPECT_EQ(repository.chats().size(), 2U);
}

}  // namespace
}  // namespace msa::core

