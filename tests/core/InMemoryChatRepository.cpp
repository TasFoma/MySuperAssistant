#include "InMemoryChatRepository.h"

#include <algorithm>

namespace msa::core {

std::int64_t InMemoryChatRepository::addChat(const Chat& chat)
{
    return m_chats.add(chat);
}

std::optional<Chat> InMemoryChatRepository::findChat(std::int64_t identifier) const
{
    return m_chats.find(identifier);
}

std::vector<Chat> InMemoryChatRepository::chats() const
{
    return m_chats.all();
}

std::optional<std::int64_t> InMemoryChatRepository::addMessage(const ChatMessage& message)
{
    if (!m_chats.find(message.chatIdentifier)) {
        return std::nullopt;
    }

    return m_messages.add(message);
}

std::vector<ChatMessage> InMemoryChatRepository::messages(std::int64_t chatIdentifier) const
{
    auto messages = m_messages.all();
    std::erase_if(messages, [chatIdentifier](const ChatMessage& message) {
        return message.chatIdentifier != chatIdentifier;
    });

    return messages;
}

std::optional<std::int64_t> InMemoryChatRepository::currentChatId() const
{
    return m_currentChatId;
}

bool InMemoryChatRepository::saveCurrentChatId(std::int64_t identifier)
{
    if (!m_chats.find(identifier)) {
        return false;
    }
    m_currentChatId = identifier;

    return true;
}

}  // namespace msa::core

