#pragma once

#include <chrono>
#include <cstdint>

namespace msa::core {

/// @brief Одно внесённое значение прогресса привычки.
struct HabitLogEntry {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Идентификатор привычки.
    std::int64_t habitIdentifier{0};
    /// @brief Дата внесения по местному календарю сервера.
    std::chrono::year_month_day date{};
    /// @brief Вклад записи в прогресс за период.
    double value{0.0};
};

}  // namespace msa::core

