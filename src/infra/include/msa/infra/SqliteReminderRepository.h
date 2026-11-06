#pragma once

#include "msa/core/ReminderRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище трёх видов напоминаний с общими постоянными ID.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteReminderRepository final : public core::ReminderRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteReminderRepository(SqliteDatabase& database);
    /// @brief Сохраняет копию с нулевым ID, назначает положительный ID.
    [[nodiscard]] std::int64_t add(const core::Reminder& item) override;
    /// @brief Возвращает копию либо nullopt при отсутствии.
    [[nodiscard]] std::optional<core::Reminder> find(std::int64_t identifier) const override;
    /// @brief Возвращает независимые копии всех видов без гарантии порядка.
    [[nodiscard]] std::vector<core::Reminder> all() const override;
    /// @brief Заменяет запись целиком, включая вид; false при отсутствии без вставки.
    [[nodiscard]] bool update(const core::Reminder& item) override;
    /// @brief Удаляет запись; false при отсутствии.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

