#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace msa::core {

/// @brief Период повторения календарного события.
enum class EventRepeat : std::uint8_t {
    None,
    Daily,
    Weekly,
    Monthly,
};

/// @brief Календарное событие по местному календарю сервера.
struct Event {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Название в UTF-8.
    std::string title{};
    /// @brief Дата первого события, в том числе для повторений.
    std::chrono::year_month_day date{};
    /// @brief Минуты от полуночи; отсутствие значения — событие без времени.
    std::optional<std::chrono::minutes> time{};
    /// @brief Описание в UTF-8.
    std::string description{};
    /// @brief Правило повторения.
    EventRepeat repeat{EventRepeat::None};
};

}  // namespace msa::core

