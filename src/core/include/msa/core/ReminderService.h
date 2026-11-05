#pragma once

#include "msa/core/Clock.h"
#include "msa/core/ReminderRepository.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Отказ создания из-за пользовательских данных.
struct ReminderRejection {
    std::string reason;
};

/// @brief Положительный ID сохранённой записи либо причина отказа.
using ReminderCreationResult = std::variant<std::int64_t, ReminderRejection>;

/// @brief Создание и обработка напоминаний по времени без таймеров и уведомлений.
class ReminderService {
public:
    /// @brief Принимает невладеющие зависимости, которые должны жить дольше сервиса.
    ReminderService(const Clock& clock, ReminderRepository& repository);
    /// @brief Создаёт однократное напоминание; прошлое время отклоняется без сохранения.
    [[nodiscard]] ReminderCreationResult addTimed(
        const std::string& title, std::chrono::local_time<std::chrono::seconds> dueAt);
    /// @brief Создаёт повторяющееся напоминание через положительный интервал от сейчас.
    [[nodiscard]] ReminderCreationResult addRecurring(
        const std::string& title, std::chrono::minutes interval);
    /// @brief При запуске помечает прошлые однократные записи Missed без уведомления.
    /// Вызывать перед первой checkDue после загрузки хранилища; повторяющиеся не меняет.
    void skipMissedOnStartup();
    /// @brief Возвращает сработавшие записи со старым dueAt после сохранения их состояния.
    /// Однократные становятся Fired, повторяющиеся переносятся на сейчас + интервал.
    /// Порядок не определён; геонапоминания не обрабатываются. Ошибки хранилища пробрасываются.
    [[nodiscard]] std::vector<Reminder> checkDue();

private:
    const Clock& m_clock;
    ReminderRepository& m_repository;
};

}  // namespace msa::core
