#pragma once

#include "msa/core/Chat.h"
#include "msa/core/ChatMessage.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Хранилище чатов, полных сообщений и сохранённого выбора текущего чата.
/// Технические ошибки передаются исключениями без Qt-типов. Автоматической очистки нет.
class ChatRepository {
public:
    /// @brief Освобождает реализацию хранилища.
    virtual ~ChatRepository();

    /// @brief Сохраняет копию чата с нулевым ID, возвращает положительный ID.
    [[nodiscard]] virtual std::int64_t addChat(const Chat& chat) = 0;
    /// @brief Возвращает копию чата либо nullopt при отсутствии.
    [[nodiscard]] virtual std::optional<Chat> findChat(std::int64_t identifier) const = 0;
    /// @brief Возвращает независимые копии всех чатов без гарантии порядка.
    [[nodiscard]] virtual std::vector<Chat> chats() const = 0;
    /// @brief Сохраняет копию сообщения с нулевым ID и назначает положительный ID.
    /// При отсутствии чата возвращает nullopt без записи; исходный объект не меняется.
    [[nodiscard]] virtual std::optional<std::int64_t> addMessage(const ChatMessage& message) = 0;
    /// @brief Возвращает копии сообщений только указанного чата без гарантии порядка.
    /// Отсутствующий или пустой чат даёт пустой список.
    [[nodiscard]] virtual std::vector<ChatMessage> messages(std::int64_t chatIdentifier) const = 0;
    /// @brief Возвращает сохранённый выбор; до первого сохранения — nullopt.
    /// Внешне загруженный ID может указывать на отсутствующий чат.
    [[nodiscard]] virtual std::optional<std::int64_t> currentChatId() const = 0;
    /// @brief Сохраняет выбор существующего чата; false при отсутствии без смены выбора.
    virtual bool saveCurrentChatId(std::int64_t identifier) = 0;
};

}  // namespace msa::core

