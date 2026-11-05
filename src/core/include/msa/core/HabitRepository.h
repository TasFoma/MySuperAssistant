#pragma once

#include "msa/core/Habit.h"
#include "msa/core/HabitLogEntry.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Хранилище привычек и их записей прогресса.
/// Технические ошибки передаются исключениями без Qt-типов; прогресс здесь не вычисляется.
class HabitRepository {
public:
    /// @brief Освобождает реализацию хранилища.
    virtual ~HabitRepository();

    /// @brief Добавляет привычку с identifier == 0; возвращает положительный ID.
    [[nodiscard]] virtual std::int64_t add(const Habit& habit) = 0;
    /// @brief Ищет привычку; отсутствие записи — std::nullopt.
    [[nodiscard]] virtual std::optional<Habit> find(std::int64_t identifier) const = 0;
    /// @brief Возвращает независимые копии привычек без гарантии порядка.
    [[nodiscard]] virtual std::vector<Habit> all() const = 0;
    /// @brief Заменяет привычку по её ID; false, если она отсутствует.
    [[nodiscard]] virtual bool update(const Habit& habit) = 0;
    /// @brief Удаляет привычку вместе с записями прогресса; false, если она отсутствует.
    [[nodiscard]] virtual bool remove(std::int64_t identifier) = 0;
    /// @brief Добавляет запись с identifier == 0 для существующей привычки.
    /// @return Положительный ID записи или std::nullopt, если привычка отсутствует.
    [[nodiscard]] virtual std::optional<std::int64_t> addLog(const HabitLogEntry& entry) = 0;
    /// @brief Возвращает копии всех записей привычки без гарантии порядка; иначе пустой список.
    [[nodiscard]] virtual std::vector<HabitLogEntry> logs(std::int64_t habitIdentifier) const = 0;
};

}  // namespace msa::core

