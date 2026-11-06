#pragma once

#include <cstdint>
#include <string>

namespace msa::core {

/// @brief Роль сообщения подготовленного запроса к языковой модели.
enum class LlmRole : std::uint8_t {
    System,
    User,
    Assistant,
};

/// @brief Сообщение запроса без привязки к HTTP или формату JSON.
struct LlmMessage {
    /// @brief Назначение сообщения в запросе.
    LlmRole role{LlmRole::User};
    /// @brief Полный текст сообщения в UTF-8.
    std::string text{};
};

}  // namespace msa::core

