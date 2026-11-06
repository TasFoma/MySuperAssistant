#pragma once

#include "msa/core/EventWarningRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище отметок предупреждений без автоматической очистки.
/// Владелец базы со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteEventWarningRepository final : public core::EventWarningRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteEventWarningRepository(SqliteDatabase& database);
    /// @brief Атомарно сохраняет ID события + дату вхождения + вид; false только для повтора.
    /// Неположительный ID, неверная дата/вид — std::invalid_argument без записи.
    /// SQL-ошибки — std::runtime_error; запись участвует во внешней транзакции базы.
    [[nodiscard]] bool markIfNew(std::int64_t identifier,
        std::chrono::year_month_day occurrenceDate, core::EventWarningKind kind) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

