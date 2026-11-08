#pragma once

#include "msa/core/ChatRepository.h"
#include "msa/core/Clock.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Причина отказа из-за пользовательских параметров операции чата.
struct ChatRejection {
    /// @brief Объяснение для вызывающей стороны.
    std::string reason;
};

/// @brief ID нового сообщения либо причина отказа.
using ChatWriteResult = std::variant<std::int64_t, ChatRejection>;
/// @brief Успех переключения либо причина отказа.
using ChatChangeResult = std::variant<std::monostate, ChatRejection>;
/// @brief Отобранные сохранённые сообщения либо отказ из-за недостаточного бюджета.
using ChatHistoryResult = std::variant<std::vector<ChatMessage>, ChatRejection>;

/// @brief Чаты, полная история и отбор её последних сообщений без генерации ответов LLM.
class ChatService {
public:
    /// @brief Восстанавливает сохранённый чат; иначе выбирает максимальный ID или создаёт чат.
    /// Новый выбор сохраняется до завершения конструктора; ошибки хранилища пробрасываются.
    /// Зависимости невладеющие и должны жить дольше сервиса.
    ChatService(const Clock& clock, ChatRepository& repository);
    /// @brief Привязывает операцию к существующему чату без чтения/изменения общего выбора.
    /// Отсутствующий чат вызывает invalid_argument; новый/выбранный чат остаётся локальным сервису.
    ChatService(const Clock& clock, ChatRepository& repository, std::int64_t chatIdentifier);
    /// @brief Возвращает ID текущего чата сервиса.
    [[nodiscard]] std::int64_t currentChatId() const;
    /// @brief Создаёт и выбирает новый чат с названием «Чат от ДД.ММ.ГГГГ ЧЧ:ММ».
    /// Выбор меняется после сохранения; при ошибке сохранённый новый чат может остаться в списке.
    [[nodiscard]] std::int64_t newChat();
    /// @brief Все чаты по убыванию ID, без изменения хранилища.
    [[nodiscard]] std::vector<Chat> chatList() const;
    /// @brief Выбирает существующий чат; при отсутствии возвращает отказ без смены выбора.
    [[nodiscard]] ChatChangeResult switchChat(std::int64_t identifier);
    /// @brief Сохраняет полное сообщение текущего чата с временем из Clock.
    /// Пустой или только пробельный текст и неизвестная роль отклоняются без записи.
    [[nodiscard]] ChatWriteResult addMessage(const std::string& text, MessageRole role);
    /// @brief Возвращает копии всех сообщений текущего чата по возрастанию ID.
    /// Не обрезает текст и не удаляет сообщения; технические ошибки пробрасываются.
    [[nodiscard]] std::vector<ChatMessage> loadMessages() const;
    /// @brief Отбирает непрерывную последнюю часть истории в пределах maxChars кодовых точек UTF-8.
    /// Только целые сообщения, по возрастанию ID; при первом непоместившемся останавливается.
    /// Если последнее сообщение не помещается — ChatRejection; пустая история допустима при нуле.
    /// Бюджет учитывает только текст, не токены модели; значение по умолчанию не задано.
    /// Хранилище не меняет; технические ошибки пробрасываются.
    [[nodiscard]] ChatHistoryResult historyMessages(std::size_t maxChars) const;

private:
    const Clock& m_clock;
    ChatRepository& m_repository;
    std::int64_t m_currentChatId{0};
    bool m_persistSelection{true};
};

}  // namespace msa::core

