#pragma once

#include <cstdint>
#include <string>

namespace msa::core {

/// @brief Период накопления прогресса привычки.
enum class HabitPeriod : std::uint8_t {
    Day,
    Week,
};

/// @brief Привычка с количественной целью за период.
struct Habit {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Название в UTF-8.
    std::string name{};
    /// @brief Целевое значение за период.
    double target{1.0};
    /// @brief Единица измерения в UTF-8, например «стр» или «мин».
    std::string unit{};
    /// @brief Период, за который считается цель.
    HabitPeriod period{HabitPeriod::Day};
};

}  // namespace msa::core

