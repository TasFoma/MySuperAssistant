#pragma once

#include "msa/core/Reminder.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Хранилище напоминаний; технические ошибки передаются исключениями без Qt-типов.
/// Сортировка и предметные правила не входят в обязанности репозитория.
class ReminderRepository {
public:
    /// @brief Освобождает реализацию хранилища.
    virtual ~ReminderRepository();

    /// @brief Добавляет запись с identifier == 0; возвращает назначенный положительный ID.
    /// Исходный объект не изменяется.
    [[nodiscard]] virtual std::int64_t add(const Reminder& item) = 0;
    /// @brief Ищет запись по ID; отсутствие записи — std::nullopt.
    [[nodiscard]] virtual std::optional<Reminder> find(std::int64_t identifier) const = 0;
    /// @brief Возвращает независимые копии всех записей без гарантии порядка.
    [[nodiscard]] virtual std::vector<Reminder> all() const = 0;
    /// @brief Заменяет запись по её ID; false, если запись отсутствует.
    [[nodiscard]] virtual bool update(const Reminder& item) = 0;
    /// @brief Удаляет запись; false, если запись отсутствует.
    [[nodiscard]] virtual bool remove(std::int64_t identifier) = 0;
};

}  // namespace msa::core

