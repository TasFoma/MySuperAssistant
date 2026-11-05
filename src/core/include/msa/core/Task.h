#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace msa::core {

/// @brief Приоритет задачи в порядке возрастания важности.
enum class TaskPriority : std::uint8_t {
    Low,
    Normal,
    High,
};

/// @brief Задача; отсутствие срока означает выполнение без привязки к дате.
struct Task {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Название в UTF-8.
    std::string title{};
    /// @brief Необязательный срок по местному календарю сервера.
    std::optional<std::chrono::year_month_day> dueDate{};
    /// @brief Важность задачи.
    TaskPriority priority{TaskPriority::Normal};
    /// @brief Признак завершения.
    bool done{false};
    /// @brief Время создания по местным часам сервера.
    std::chrono::local_time<std::chrono::seconds> createdAt{};
};

}  // namespace msa::core

