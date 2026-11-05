#pragma once

#include "msa/core/EventRepository.h"

#include <chrono>
#include <vector>

namespace msa::core {

/// @brief Чтение календаря с учётом повторений, без изменения хранилища.
class CalendarService {
public:
    /// @brief Принимает невладеющую ссылку на репозиторий, живущий дольше сервиса.
    explicit CalendarService(const EventRepository& repository);
    /// @brief События дня: без времени первыми, затем по времени; равные — по ID.
    /// Возвращает копии с исходной датой начала повторения. Неверная дата — пустой список.
    [[nodiscard]] std::vector<Event> eventsForDate(std::chrono::year_month_day date) const;
    /// @brief Уникальные дни месяца с событиями по возрастанию; неверный месяц — пустой список.
    [[nodiscard]] std::vector<std::chrono::day> eventDaysInMonth(std::chrono::year_month month) const;

private:
    const EventRepository& m_repository;
};

}  // namespace msa::core
