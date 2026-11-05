#pragma once

#include <chrono>
#include <cstdint>

namespace msa::core {

/// @brief Вид предупреждения для отдельного вхождения события.
enum class EventWarningKind : std::uint8_t {
    BeforeStart,
    DayBefore,
    OnDay,
};

/// @brief Хранилище отметок подготовленных предупреждений; ошибки — исключениями.
class EventWarningRepository {
public:
    /// @brief Освобождает хранилище.
    virtual ~EventWarningRepository();
    /// @brief Сохраняет новую отметку ID + дата вхождения + вид; true только при первом сохранении.
    /// Повторный вызов не меняет отметку и возвращает false. Дата — день события, не выдачи.
    [[nodiscard]] virtual bool markIfNew(std::int64_t identifier,
        std::chrono::year_month_day occurrenceDate, EventWarningKind kind) = 0;
};

}  // namespace msa::core

