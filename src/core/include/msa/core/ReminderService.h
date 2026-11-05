#pragma once

#include "msa/core/Clock.h"
#include "msa/core/ReminderRepository.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Отказ операции из-за пользовательских данных.
struct ReminderRejection {
    std::string reason;
};

/// @brief Положительный ID сохранённой записи либо причина отказа.
using ReminderCreationResult = std::variant<std::int64_t, ReminderRejection>;

/// @brief Исходные сработавшие геонапоминания либо отказ из-за неверной позиции.
using PlaceCheckResult = std::variant<std::vector<PlaceReminder>, ReminderRejection>;

/// @brief Создание и обработка напоминаний по времени и месту без доставки уведомлений.
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
    /// @brief Создаёт геонапоминание Pending; название и место непустые.
    /// Координаты необязательны; конечные широта и долгота лежат в пределах ±90 и ±180.
    [[nodiscard]] ReminderCreationResult addPlace(const std::string& title,
        const std::string& place, std::optional<GeoCoordinates> coordinates = std::nullopt);
    /// @brief При расстоянии строго меньше 200 м сохраняет Fired и возвращает исходную запись.
    /// Проверяет только Pending с координатами; порядок не определён.
    /// Неверная текущая позиция — отказ без изменений; неверные сохранённые координаты — исключение.
    [[nodiscard]] PlaceCheckResult checkNearPlace(GeoCoordinates position);
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
