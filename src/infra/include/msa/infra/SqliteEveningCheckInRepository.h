#pragma once

#include "msa/core/EveningCheckInRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище независимых даты выдачи опроса и ожидания ответа.
/// Владелец базы со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteEveningCheckInRepository final : public core::EveningCheckInRepository {
public:
    /// @brief Сохраняет невладеющую ссылку на базу с предметной схемой.
    explicit SqliteEveningCheckInRepository(SqliteDatabase& database);
    /// @brief Возвращает дату выдачи либо nullopt до первого сохранения.
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override;
    /// @brief Заменяет дату, не меняя ожидание; неверная дата — std::invalid_argument.
    void saveDate(std::chrono::year_month_day date) override;
    /// @brief Возвращает копию ожидания либо nullopt; истечение проверяет core.
    [[nodiscard]] std::optional<core::PendingEveningCheckIn> pendingCheckIn() const override;
    /// @brief Заменяет ожидание или снимает его через nullopt, не меняя дату выдачи.
    /// Неверная дата/неположительный ID — std::invalid_argument без записи.
    /// Сообщение должно существовать в указанном чате; SQL-ошибки — std::runtime_error.
    /// Все записи участвуют во внешней транзакции SqliteDatabase.
    void savePendingCheckIn(std::optional<core::PendingEveningCheckIn> pending) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

