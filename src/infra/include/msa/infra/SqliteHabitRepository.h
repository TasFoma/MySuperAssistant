#pragma once

#include "msa/core/HabitRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище привычек и полного журнала без вычисления прогресса.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteHabitRepository final : public core::HabitRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteHabitRepository(SqliteDatabase& database);
    /// @brief Сохраняет копию новой привычки с нулевым ID, назначает положительный ID.
    [[nodiscard]] std::int64_t add(const core::Habit& habit) override;
    /// @brief Возвращает копию привычки либо nullopt.
    [[nodiscard]] std::optional<core::Habit> find(std::int64_t identifier) const override;
    /// @brief Возвращает независимые копии привычек без гарантии порядка.
    [[nodiscard]] std::vector<core::Habit> all() const override;
    /// @brief Заменяет привычку без изменения журнала; false при отсутствии.
    [[nodiscard]] bool update(const core::Habit& habit) override;
    /// @brief Атомарно удаляет привычку и её журнал; false при отсутствии.
    [[nodiscard]] bool remove(std::int64_t identifier) override;
    /// @brief Сохраняет новую отметку с нулевым ID; nullopt при отсутствии привычки.
    [[nodiscard]] std::optional<std::int64_t> addLog(const core::HabitLogEntry& entry) override;
    /// @brief Возвращает копии всех отметок привычки; иначе пустой список.
    [[nodiscard]] std::vector<core::HabitLogEntry> logs(std::int64_t habitIdentifier) const override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

