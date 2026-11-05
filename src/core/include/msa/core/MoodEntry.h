#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace msa::core {

/// @brief Явно указанная оценка настроения пользователя.
enum class MoodScore : std::int8_t {
    Negative = -1,
    Neutral = 0,
    Positive = 1,
};

/// @brief Запись дневника настроения с полным текстом в UTF-8.
struct MoodEntry {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Местное время записи на сервере с точностью до секунд.
    std::chrono::local_time<std::chrono::seconds> recordedAt{};
    /// @brief Оценка, переданная при явной записи настроения.
    MoodScore score{MoodScore::Neutral};
    /// @brief Полная заметка без обрезки и изменения пробелов; может быть пустой.
    std::string note{};
};

}  // namespace msa::core

