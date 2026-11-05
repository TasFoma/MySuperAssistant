#pragma once

#include "msa/core/GeoCoordinates.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace msa::core {

/// @brief Однократное напоминание по местным часам сервера.
struct TimedReminder {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Текст напоминания в UTF-8.
    std::string title{};
    /// @brief Момент срабатывания.
    std::chrono::local_time<std::chrono::seconds> dueAt{};
    /// @brief Было ли напоминание уже выдано.
    bool fired{false};
};

/// @brief Напоминание с интервалом и следующим моментом срабатывания.
struct RecurringReminder {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Текст напоминания в UTF-8.
    std::string title{};
    /// @brief Следующее срабатывание по местным часам сервера.
    std::chrono::local_time<std::chrono::seconds> dueAt{};
    /// @brief Положительный интервал между срабатываниями.
    std::chrono::minutes interval{};
};

/// @brief Однократное напоминание при приближении к месту.
struct PlaceReminder {
    /// @brief Идентификатор записи; ноль — ещё не сохранена.
    std::int64_t identifier{0};
    /// @brief Текст напоминания в UTF-8.
    std::string title{};
    /// @brief Название или адрес места в UTF-8.
    std::string place{};
    /// @brief Координаты; отсутствуют, пока место не определено.
    std::optional<GeoCoordinates> coordinates{};
    /// @brief Было ли напоминание уже выдано.
    bool fired{false};
};

/// @brief Напоминание одного из трёх поддерживаемых видов.
using Reminder = std::variant<TimedReminder, RecurringReminder, PlaceReminder>;

}  // namespace msa::core

