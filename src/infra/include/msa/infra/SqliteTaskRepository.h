#pragma once

#include "msa/core/TaskRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище задач без предметной сортировки и автоматической очистки.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteTaskRepository final : public core::TaskRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteTaskRepository(SqliteDatabase& database);
    /// @brief Сохраняет копию задачи с нулевым ID и назначает положительный ID.
    [[nodiscard]] std::int64_t add(const core::Task& item) override;
    /// @brief Возвращает копию задачи либо nullopt.
    [[nodiscard]] std::optional<core::Task> find(std::int64_t identifier) const override;
    /// @brief Возвращает независимые копии всех задач без гарантии порядка.
    [[nodiscard]] std::vector<core::Task> all() const override;
    /// @brief Заменяет все поля по ID; false при отсутствии без вставки.
    [[nodiscard]] bool update(const core::Task& item) override;
    /// @brief Удаляет задачу по ID; false при отсутствии.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

