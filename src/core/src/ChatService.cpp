#include "msa/core/ChatService.h"

#include "PlannerText.h"
#include "Utf8Text.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>

namespace msa::core {

ChatService::ChatService(const Clock& clock, ChatRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
    const auto saved = m_repository.currentChatId();
    if (saved && m_repository.findChat(*saved)) {
        m_currentChatId = *saved;
        return;
    }
    const auto chats = m_repository.chats();
    if (chats.empty()) {
        static_cast<void>(newChat());
        return;
    }
    const auto latest = std::ranges::max_element(chats, {}, &Chat::identifier)->identifier;
    if (!m_repository.saveCurrentChatId(latest)) {
        throw std::runtime_error{"Чат исчез при восстановлении выбора"};
    }
    m_currentChatId = latest;
}

std::int64_t ChatService::currentChatId() const
{
    return m_currentChatId;
}

std::int64_t ChatService::newChat()
{
    const auto now = m_clock.now();
    const auto dayStart = std::chrono::floor<std::chrono::days>(now);
    const auto date = std::chrono::year_month_day{std::chrono::sys_days{dayStart.time_since_epoch()}};
    const auto title = std::format("Чат от {:02}.{:02}.{:04} {}",
        static_cast<unsigned>(date.day()), static_cast<unsigned>(date.month()),
        static_cast<int>(date.year()),
        text::timeText(std::chrono::duration_cast<std::chrono::minutes>(now - dayStart)));
    const auto identifier = m_repository.addChat(Chat{.title = title, .createdAt = now});
    if (!m_repository.saveCurrentChatId(identifier)) {
        throw std::runtime_error{"Новый чат исчез при сохранении выбора"};
    }
    m_currentChatId = identifier;

    return identifier;
}

std::vector<Chat> ChatService::chatList() const
{
    auto chats = m_repository.chats();
    std::ranges::sort(chats, [](const Chat& left, const Chat& right) {
        return left.identifier > right.identifier;
    });

    return chats;
}

ChatChangeResult ChatService::switchChat(std::int64_t identifier)
{
    if (!m_repository.saveCurrentChatId(identifier)) {
        return ChatRejection{"Чат не найден."};
    }
    m_currentChatId = identifier;

    return std::monostate{};
}

ChatWriteResult ChatService::addMessage(const std::string& messageText, MessageRole role)
{
    if (text::trim(messageText).empty()) {
        return ChatRejection{"Введите непустое сообщение."};
    }
    if (role != MessageRole::User && role != MessageRole::Assistant) {
        return ChatRejection{"Выберите допустимого автора сообщения."};
    }
    const auto identifier = m_repository.addMessage(ChatMessage{
        .chatIdentifier = m_currentChatId, .role = role, .text = messageText, .createdAt = m_clock.now()});
    if (!identifier) {
        return ChatRejection{"Чат не найден."};
    }

    return *identifier;
}

std::vector<ChatMessage> ChatService::loadMessages() const
{
    auto messages = m_repository.messages(m_currentChatId);
    std::ranges::sort(messages, {}, &ChatMessage::identifier);

    return messages;
}

}  // namespace msa::core

