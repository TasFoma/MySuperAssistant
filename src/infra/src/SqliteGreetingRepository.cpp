#include "msa/infra/SqliteGreetingRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <optional>
#include <stdexcept>

namespace msa::infra {

SqliteGreetingRepository::SqliteGreetingRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::optional<std::chrono::year_month_day> SqliteGreetingRepository::lastDate() const
{
    QSqlQuery query{m_database.connection()};
    if (!query.exec(QStringLiteral("SELECT year, month, day FROM greeting_state WHERE identifier = 1"))) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    if (!query.next()) {
        if (query.lastError().isValid()) {
            throw std::runtime_error{query.lastError().text().toStdString()};
        }

        return std::nullopt;
    }
    const auto date = std::chrono::year{query.value(0).toInt()}
        / std::chrono::month{query.value(1).toUInt()} / std::chrono::day{query.value(2).toUInt()};
    if (!date.ok()) {
        throw std::runtime_error{"В SQLite сохранена некорректная дата приветствия"};
    }

    return date;
}

void SqliteGreetingRepository::saveDate(std::chrono::year_month_day date)
{
    if (!date.ok()) {
        throw std::invalid_argument{"Укажите корректную дату приветствия"};
    }
    QSqlQuery query{m_database.connection()};
    if (!query.prepare(QStringLiteral("INSERT INTO greeting_state(identifier, year, month, day) "
                                      "VALUES (1, ?, ?, ?) ON CONFLICT(identifier) DO UPDATE "
                                      "SET year = excluded.year, month = excluded.month, day = excluded.day"))) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    query.addBindValue(static_cast<int>(date.year()));
    query.addBindValue(static_cast<unsigned>(date.month()));
    query.addBindValue(static_cast<unsigned>(date.day()));
    if (!query.exec()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

}  // namespace msa::infra

