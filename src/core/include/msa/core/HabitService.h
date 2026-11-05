#pragma once

#include "msa/core/Clock.h"
#include "msa/core/HabitRepository.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Причина отказа операции с привычкой.
struct HabitRejection {
    std::string reason;
};

/// @brief ID созданной привычки или записи журнала либо причина отказа.
using HabitWriteResult = std::variant<std::int64_t, HabitRejection>;

/// @brief Привычка и сумма отметок за текущий период без ограничения целью.
struct HabitProgress {
    Habit habit;
    double progress{0.0};
};

/// @brief Создание привычек, отметки за сегодня и расчёт текущего прогресса.
class HabitService {
public:
    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    HabitService(const Clock& clock, HabitRepository& repository);
    /// @brief Создаёт привычку; цель должна быть положительным конечным числом.
    [[nodiscard]] HabitWriteResult addHabit(const std::string& name, double target,
                                          const std::string& unit, HabitPeriod period);
    /// @brief Добавляет положительное конечное значение за местный сегодняшний день.
    /// Отсутствующая привычка возвращается как отказ с причиной.
    [[nodiscard]] HabitWriteResult logHabit(std::int64_t identifier, double value);
    /// @brief Список по ID с прогрессом за сегодня либо с понедельника по сегодня включительно.
    /// Будущие записи не учитываются; технические ошибки хранилища пробрасываются.
    [[nodiscard]] std::vector<HabitProgress> habits() const;

private:
    const Clock& m_clock;
    HabitRepository& m_repository;
};

}  // namespace msa::core
