#pragma once

#include "InMemoryStore.h"

#include "msa/core/ChatRepository.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Хранилище чатов, сообщений и выбора в памяти для тестов.
class InMemoryChatRepository final : public ChatRepository {
public:
    /// @brief Сохраняет копию нового чата и назначает ID.
    [[nodiscard]] std::int64_t addChat(const Chat& chat) override;
    /// @brief Возвращает копию чата или nullopt.
    [[nodiscard]] std::optional<Chat> findChat(std::int64_t identifier) const override;
    /// @brief Возвращает копии всех чатов.
    [[nodiscard]] std::vector<Chat> chats() const override;
    /// @brief Добавляет сообщение существующего чата или возвращает nullopt.
    [[nodiscard]] std::optional<std::int64_t> addMessage(const ChatMessage& message) override;
    /// @brief Возвращает копии сообщений выбранного чата.
    [[nodiscard]] std::vector<ChatMessage> messages(std::int64_t chatIdentifier) const override;
    /// @brief Возвращает сохранённый выбор.
    [[nodiscard]] std::optional<std::int64_t> currentChatId() const override;
    /// @brief Сохраняет выбор только существующего чата.
    bool saveCurrentChatId(std::int64_t identifier) override;

private:
    InMemoryStore<Chat> m_chats{};
    InMemoryStore<ChatMessage> m_messages{};
    std::optional<std::int64_t> m_currentChatId{};
};

}  // namespace msa::core

