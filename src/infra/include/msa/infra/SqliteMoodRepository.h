#pragma once

#include "msa/core/MoodRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-дневник с полным текстом заметок, без автоматической очистки.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteMoodRepository final : public core::MoodRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteMoodRepository(SqliteDatabase& database);
    /// @brief Сохраняет копию записи, назначает положительный ID, не меняя исходник.
    /// Ненулевой входной ID/неизвестная оценка — std::invalid_argument без записи.
    /// SQL-ошибки — std::runtime_error; участвует во внешней транзакции базы.
    [[nodiscard]] std::int64_t add(const core::MoodEntry& entry) override;
    /// @brief Возвращает копии всех записей без гарантии порядка.
    /// SQL-ошибки и неверная сохранённая оценка — std::runtime_error.
    [[nodiscard]] std::vector<core::MoodEntry> all() const override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

