#pragma once

#include "msa/core/CalendarService.h"
#include "msa/core/Clock.h"
#include "msa/core/EventWarningRepository.h"

#include <chrono>
#include <vector>

namespace msa::core {

/// @brief Данные предупреждения; event.date остаётся исходной датой серии.
struct EventWarning {
    Event event;
    std::chrono::year_month_day occurrenceDate;
    EventWarningKind kind;
};

/// @brief Подготовка предупреждений с сохранением отметок до возврата, без доставки.
class EventWarningService {
public:
    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    EventWarningService(const Clock& clock, const CalendarService& calendar,
        EventWarningRepository& repository);
    /// @brief Со временем: 0 < до начала <= 60 минут, включая события после полуночи.
    /// Без времени: с 08:00 накануне и в день события, один раз для каждого вида.
    /// Повторы учитывает календарь. Порядок не гарантируется; ошибки пробрасываются.
    [[nodiscard]] std::vector<EventWarning> checkDue();

private:
    const Clock& m_clock;
    const CalendarService& m_calendar;
    EventWarningRepository& m_repository;
};

}  // namespace msa::core

