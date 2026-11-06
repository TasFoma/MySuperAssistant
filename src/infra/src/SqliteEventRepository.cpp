#include "msa/infra/SqliteEventRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <stdexcept>

namespace msa::infra {
namespace {

constexpr auto kDayDuration = std::chrono::hours{24};

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

[[nodiscard]] bool validFields(const core::Event& item)
{
    return item.date.ok()
        && (!item.time || (*item.time >= std::chrono::minutes::zero() && *item.time < kDayDuration))
        && (item.repeat == core::EventRepeat::None || item.repeat == core::EventRepeat::Daily
            || item.repeat == core::EventRepeat::Weekly || item.repeat == core::EventRepeat::Monthly);
}

void bindEvent(QSqlQuery& query, const core::Event& item)
{
    if (!validFields(item)) {
        throw std::invalid_argument{"Укажите корректные дату, время и повтор события"};
    }
    query.addBindValue(QString::fromStdString(item.title));
    query.addBindValue(static_cast<int>(item.date.year()));
    query.addBindValue(static_cast<unsigned>(item.date.month()));
    query.addBindValue(static_cast<unsigned>(item.date.day()));
    query.addBindValue(item.time ? QVariant::fromValue<qlonglong>(item.time->count()) : QVariant{});
    query.addBindValue(QString::fromStdString(item.description));
    query.addBindValue(static_cast<int>(item.repeat));
}

[[nodiscard]] core::Event readEvent(const QSqlQuery& query)
{
    // До сужения к enum проверяем число: неизвестное значение не должно стать допустимым.
    const auto repeat = query.value(7).toInt();
    if (repeat < static_cast<int>(core::EventRepeat::None)
        || repeat > static_cast<int>(core::EventRepeat::Monthly)) {
        throw std::runtime_error{"В SQLite сохранён неизвестный повтор события"};
    }
    core::Event item{.identifier = query.value(0).toLongLong(),
        .title = query.value(1).toString().toStdString(),
        .date = std::chrono::year{query.value(2).toInt()}
            / std::chrono::month{query.value(3).toUInt()} / std::chrono::day{query.value(4).toUInt()},
        .description = query.value(6).toString().toStdString(), .repeat = static_cast<core::EventRepeat>(repeat)};
    if (!query.value(5).isNull()) {
        item.time = std::chrono::minutes{query.value(5).toLongLong()};
    }
    if (!validFields(item)) {
        throw std::runtime_error{"В SQLite сохранены некорректные поля события"};
    }

    return item;
}

}  // namespace

SqliteEventRepository::SqliteEventRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteEventRepository::add(const core::Event& item)
{
    if (item.identifier != 0) {
        throw std::invalid_argument{"Новое событие должно иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO events(title, year, month, day, time_minutes, description, repeat) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    bindEvent(query, item);
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::optional<core::Event> SqliteEventRepository::find(std::int64_t identifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, year, month, day, time_minutes, description, repeat "
                                  "FROM events WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readEvent(query);
}

std::vector<core::Event> SqliteEventRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, year, month, day, time_minutes, description, repeat "
                                  "FROM events"));
    execute(query);
    std::vector<core::Event> result;
    while (next(query)) {
        result.push_back(readEvent(query));
    }

    return result;
}

bool SqliteEventRepository::update(const core::Event& item)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("UPDATE events SET title = ?, year = ?, month = ?, day = ?, time_minutes = ?, "
                                  "description = ?, repeat = ? WHERE identifier = ?"));
    bindEvent(query, item);
    query.addBindValue(QVariant::fromValue<qlonglong>(item.identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

bool SqliteEventRepository::remove(std::int64_t identifier)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("DELETE FROM events WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

}  // namespace msa::infra

