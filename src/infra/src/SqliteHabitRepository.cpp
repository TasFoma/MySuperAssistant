#include "msa/infra/SqliteHabitRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <cmath>
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

[[nodiscard]] bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

void bindHabit(QSqlQuery& query, const core::Habit& habit)
{
    if (!positiveFinite(habit.target)
        || (habit.period != core::HabitPeriod::Day && habit.period != core::HabitPeriod::Week)) {
        throw std::invalid_argument{"Укажите положительную конечную цель и допустимый период привычки"};
    }
    query.addBindValue(QString::fromStdString(habit.name));
    query.addBindValue(habit.target);
    query.addBindValue(QString::fromStdString(habit.unit));
    query.addBindValue(static_cast<int>(habit.period));
}

[[nodiscard]] core::Habit readHabit(const QSqlQuery& query)
{
    const auto target = query.value(2).toDouble();
    const auto period = query.value(4).toInt();
    if (!positiveFinite(target)
        || (period != static_cast<int>(core::HabitPeriod::Day) && period != static_cast<int>(core::HabitPeriod::Week))) {
        throw std::runtime_error{"В SQLite сохранены некорректные цель или период привычки"};
    }

    return {.identifier = query.value(0).toLongLong(), .name = query.value(1).toString().toStdString(),
        .target = target, .unit = query.value(3).toString().toStdString(), .period = static_cast<core::HabitPeriod>(period)};
}

}  // namespace

SqliteHabitRepository::SqliteHabitRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteHabitRepository::add(const core::Habit& habit)
{
    if (habit.identifier != 0) {
        throw std::invalid_argument{"Новая привычка должна иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO habits(name, target, unit, period) VALUES (?, ?, ?, ?)"));
    bindHabit(query, habit);
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::optional<core::Habit> SqliteHabitRepository::find(std::int64_t identifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, name, target, unit, period FROM habits WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readHabit(query);
}

std::vector<core::Habit> SqliteHabitRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, name, target, unit, period FROM habits"));
    execute(query);
    std::vector<core::Habit> result;
    while (next(query)) {
        result.push_back(readHabit(query));
    }

    return result;
}

bool SqliteHabitRepository::update(const core::Habit& habit)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("UPDATE habits SET name = ?, target = ?, unit = ?, period = ? WHERE identifier = ?"));
    bindHabit(query, habit);
    query.addBindValue(QVariant::fromValue<qlonglong>(habit.identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

bool SqliteHabitRepository::remove(std::int64_t identifier)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("DELETE FROM habits WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

std::optional<std::int64_t> SqliteHabitRepository::addLog(const core::HabitLogEntry& entry)
{
    if (entry.identifier != 0 || !entry.date.ok() || !positiveFinite(entry.value)) {
        throw std::invalid_argument{"Укажите нулевой ID отметки, корректную дату и положительное конечное значение"};
    }
    QSqlQuery query{m_database.connection()};
    // Проверка наличия привычки и вставка составляют одну атомарную операцию.
    prepare(query, QStringLiteral("INSERT INTO habit_logs(habit_identifier, year, month, day, value) "
                                  "SELECT identifier, ?, ?, ?, ? FROM habits WHERE identifier = ?"));
    query.addBindValue(static_cast<int>(entry.date.year()));
    query.addBindValue(static_cast<unsigned>(entry.date.month()));
    query.addBindValue(static_cast<unsigned>(entry.date.day()));
    query.addBindValue(entry.value);
    query.addBindValue(QVariant::fromValue<qlonglong>(entry.habitIdentifier));
    execute(query);
    if (query.numRowsAffected() == 0) {
        return std::nullopt;
    }

    return query.lastInsertId().toLongLong();
}

std::vector<core::HabitLogEntry> SqliteHabitRepository::logs(std::int64_t habitIdentifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, habit_identifier, year, month, day, value "
                                  "FROM habit_logs WHERE habit_identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(habitIdentifier));
    execute(query);
    std::vector<core::HabitLogEntry> result;
    while (next(query)) {
        core::HabitLogEntry entry{.identifier = query.value(0).toLongLong(),
            .habitIdentifier = query.value(1).toLongLong(),
            .date = std::chrono::year{query.value(2).toInt()}
                / std::chrono::month{query.value(3).toUInt()} / std::chrono::day{query.value(4).toUInt()},
            .value = query.value(5).toDouble()};
        if (!entry.date.ok() || !positiveFinite(entry.value)) {
            throw std::runtime_error{"В SQLite сохранена некорректная отметка привычки"};
        }
        result.push_back(entry);
    }

    return result;
}

}  // namespace msa::infra

