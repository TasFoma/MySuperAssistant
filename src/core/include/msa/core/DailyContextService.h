#pragma once

#include "msa/core/CalendarService.h"
#include "msa/core/Clock.h"
#include "msa/core/HabitService.h"
#include "msa/core/TaskService.h"

#include <string>

namespace msa::core {

/// @brief Формирует текст сводки дня без профиля, истории чата и обращения к LLM.
class DailyContextService {
public:
    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    /// Clock и часы HabitService должны представлять одно местное время сервера.
    DailyContextService(const Clock& clock, const CalendarService& calendar,
                        const TaskService& tasks, const HabitService& habits);
    /// @brief Дата и время, события сегодня, до пяти открытых задач и все привычки.
    /// Пустые разделы пропускаются; повторные вызовы разрешены, записи не меняются.
    /// Технические исключения зависимостей пробрасываются вызывающему коду.
    [[nodiscard]] std::string dailyContext() const;

private:
    const Clock& m_clock;
    const CalendarService& m_calendar;
    const TaskService& m_tasks;
    const HabitService& m_habits;
};

}  // namespace msa::core

