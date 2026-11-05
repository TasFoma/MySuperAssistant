#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace msa::core {

/// @brief Сохранённый чат пользователя.
struct Chat {
    /// @brief Идентификатор; ноль — чат ещё не сохранён.
    std::int64_t identifier{0};
    /// @brief Название чата в UTF-8.
    std::string title{};
    /// @brief Местное время создания на сервере с точностью до секунд.
    std::chrono::local_time<std::chrono::seconds> createdAt{};
};

}  // namespace msa::core

