#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace msa::core {

/// @brief Автор сохранённого сообщения чата.
enum class MessageRole : std::uint8_t {
    User,
    Assistant,
};

/// @brief Полное сообщение одного чата; ограничения контекста LLM к нему не применяются.
struct ChatMessage {
    /// @brief Идентификатор сообщения; ноль — ещё не сохранено.
    std::int64_t identifier{0};
    /// @brief Идентификатор чата, которому принадлежит сообщение.
    std::int64_t chatIdentifier{0};
    /// @brief Автор сообщения.
    MessageRole role{MessageRole::User};
    /// @brief Полный исходный текст в UTF-8.
    std::string text{};
    /// @brief Местное время записи на сервере с точностью до секунд.
    std::chrono::local_time<std::chrono::seconds> createdAt{};
};

}  // namespace msa::core

