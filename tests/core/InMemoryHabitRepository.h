#pragma once

#include "InMemoryStore.h"

#include "msa/core/HabitRepository.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Тестовое хранилище Habit без файлов и внешних сервисов.
class InMemoryHabitRepository final : public HabitRepository {
public:
    /// @brief Добавляет запись и возвращает назначенный ID.
    [[nodiscard]] std::int64_t add(const Habit& item) override;
    /// @brief Возвращает копию записи или std::nullopt.
    [[nodiscard]] std::optional<Habit> find(std::int64_t identifier) const override;
    /// @brief Возвращает копии всех записей.
    [[nodiscard]] std::vector<Habit> all() const override;
    /// @brief Заменяет существующую запись.
    [[nodiscard]] bool update(const Habit& item) override;
    /// @brief Удаляет запись вместе с журналом.
    [[nodiscard]] bool remove(std::int64_t identifier) override;
    /// @brief Добавляет запись прогресса для существующей привычки.
    [[nodiscard]] std::optional<std::int64_t> addLog(const HabitLogEntry& entry) override;
    /// @brief Возвращает записи прогресса указанной привычки.
    [[nodiscard]] std::vector<HabitLogEntry> logs(std::int64_t habitIdentifier) const override;

private:
    InMemoryStore<Habit> m_items{};
    InMemoryStore<HabitLogEntry> m_logs{};
};

}  // namespace msa::core

