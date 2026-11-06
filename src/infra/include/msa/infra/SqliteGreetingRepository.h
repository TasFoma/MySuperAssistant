#pragma once

#include "msa/core/GreetingRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище даты последнего подготовленного приветствия.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteGreetingRepository final : public core::GreetingRepository {
public:
    /// @brief Сохраняет невладеющую ссылку на базу с предметной схемой.
    explicit SqliteGreetingRepository(SqliteDatabase& database);
    /// @brief Возвращает календарную дату; до первого сохранения — nullopt.
    /// Ошибка SQL или некорректная сохранённая дата — std::runtime_error.
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override;
    /// @brief Заменяет дату одним запросом; участвует во внешней транзакции базы.
    /// Некорректная дата — std::invalid_argument без записи; SQL-ошибка — std::runtime_error.
    void saveDate(std::chrono::year_month_day date) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

