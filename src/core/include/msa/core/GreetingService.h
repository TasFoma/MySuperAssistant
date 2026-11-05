#pragma once

#include "msa/core/CalendarService.h"
#include "msa/core/Clock.h"
#include "msa/core/GreetingRepository.h"
#include "msa/core/TaskRepository.h"

#include <optional>
#include <string>

namespace msa::core {

/// @brief Подготавливает приветствие с планами один раз за местный календарный день.
class GreetingService {
public:
    /// @brief Все зависимости невладеющие и должны жить дольше сервиса.
    GreetingService(const Clock& clock, const CalendarService& calendar,
                    const TaskRepository& tasks, GreetingRepository& greetings);
    /// @brief Возвращает текст с датой, событиями и максимум пятью открытыми задачами.
    /// До возврата сохраняет дату; повторный вызов в тот же день даёт nullopt.
    /// Ошибки зависимостей пробрасываются. Метод не отправляет сообщение клиентам.
    [[nodiscard]] std::optional<std::string> greetingIfNeeded();

private:
    const Clock& m_clock;
    const CalendarService& m_calendar;
    const TaskRepository& m_tasks;
    GreetingRepository& m_greetings;
};

}  // namespace msa::core
