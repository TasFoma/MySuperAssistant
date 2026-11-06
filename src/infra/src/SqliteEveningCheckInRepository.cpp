#include "msa/infra/SqliteEveningCheckInRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <stdexcept>

namespace msa::infra {
namespace {

void prepare(QSqlQuery& query, const QString& statement)
{
    if (!query.prepare(statement)) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

void execute(QSqlQuery& query)
{
    if (!query.exec()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

[[nodiscard]] bool next(QSqlQuery& query)
{
    if (query.next()) {
        return true;
    }
    if (query.lastError().isValid()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }

    return false;
}

[[nodiscard]] std::chrono::year_month_day readDate(const QSqlQuery& query)
{
    const auto date = std::chrono::year{query.value(0).toInt()}
        / std::chrono::month{query.value(1).toUInt()} / std::chrono::day{query.value(2).toUInt()};
    if (!date.ok()) {
        throw std::runtime_error{"В SQLite сохранена некорректная дата вечернего опроса"};
    }

    return date;
}

void bindDate(QSqlQuery& query, std::chrono::year_month_day date)
{
    query.addBindValue(static_cast<int>(date.year()));
    query.addBindValue(static_cast<unsigned>(date.month()));
    query.addBindValue(static_cast<unsigned>(date.day()));
}

}  // namespace

SqliteEveningCheckInRepository::SqliteEveningCheckInRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::optional<std::chrono::year_month_day> SqliteEveningCheckInRepository::lastDate() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT year, month, day FROM evening_check_in_state WHERE identifier = 1"));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readDate(query);
}

void SqliteEveningCheckInRepository::saveDate(std::chrono::year_month_day date)
{
    if (!date.ok()) {
        throw std::invalid_argument{"Укажите корректную дату вечернего опроса"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO evening_check_in_state(identifier, year, month, day) "
                                  "VALUES (1, ?, ?, ?) ON CONFLICT(identifier) DO UPDATE "
                                  "SET year = excluded.year, month = excluded.month, day = excluded.day"));
    bindDate(query, date);
    execute(query);
}

std::optional<core::PendingEveningCheckIn> SqliteEveningCheckInRepository::pendingCheckIn() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT year, month, day, chat_identifier, message_identifier "
                                  "FROM pending_evening_check_in WHERE identifier = 1"));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return core::PendingEveningCheckIn{.chatIdentifier = query.value(3).toLongLong(),
        .messageIdentifier = query.value(4).toLongLong(), .date = readDate(query)};
}

void SqliteEveningCheckInRepository::savePendingCheckIn(std::optional<core::PendingEveningCheckIn> pending)
{
    if (!pending) {
        m_database.execute(QStringLiteral("DELETE FROM pending_evening_check_in WHERE identifier = 1"));
        return;
    }
    if (!pending->date.ok() || pending->chatIdentifier <= 0 || pending->messageIdentifier <= 0) {
        throw std::invalid_argument{"Укажите корректную дату и сохранённое сообщение вечернего опроса"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO pending_evening_check_in "
                                  "(identifier, year, month, day, chat_identifier, message_identifier) "
                                  "VALUES (1, ?, ?, ?, ?, ?) ON CONFLICT(identifier) DO UPDATE "
                                  "SET year = excluded.year, month = excluded.month, day = excluded.day, "
                                  "chat_identifier = excluded.chat_identifier, "
                                  "message_identifier = excluded.message_identifier"));
    bindDate(query, pending->date);
    query.addBindValue(QVariant::fromValue<qlonglong>(pending->chatIdentifier));
    query.addBindValue(QVariant::fromValue<qlonglong>(pending->messageIdentifier));
    execute(query);
}

}  // namespace msa::infra

