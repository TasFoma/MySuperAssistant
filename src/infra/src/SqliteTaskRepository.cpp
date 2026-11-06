#include "msa/infra/SqliteTaskRepository.h"

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

[[nodiscard]] bool validPriority(int priority)
{
    return priority == static_cast<int>(core::TaskPriority::Low)
        || priority == static_cast<int>(core::TaskPriority::Normal)
        || priority == static_cast<int>(core::TaskPriority::High);
}

void bindTask(QSqlQuery& query, const core::Task& item)
{
    if ((item.dueDate && !item.dueDate->ok()) || !validPriority(static_cast<int>(item.priority))) {
        throw std::invalid_argument{"Укажите корректный срок и допустимый приоритет задачи"};
    }
    query.addBindValue(QString::fromStdString(item.title));
    query.addBindValue(item.dueDate ? QVariant{static_cast<int>(item.dueDate->year())} : QVariant{});
    query.addBindValue(item.dueDate ? QVariant{static_cast<unsigned>(item.dueDate->month())} : QVariant{});
    query.addBindValue(item.dueDate ? QVariant{static_cast<unsigned>(item.dueDate->day())} : QVariant{});
    query.addBindValue(static_cast<int>(item.priority));
    query.addBindValue(item.done);
    query.addBindValue(QVariant::fromValue<qlonglong>(item.createdAt.time_since_epoch().count()));
}

[[nodiscard]] core::Task readTask(const QSqlQuery& query)
{
    const auto priority = query.value(5).toInt();
    if (!validPriority(priority)) {
        throw std::runtime_error{"В SQLite сохранён неизвестный приоритет задачи"};
    }
    core::Task item{.identifier = query.value(0).toLongLong(), .title = query.value(1).toString().toStdString(),
        .priority = static_cast<core::TaskPriority>(priority), .done = query.value(6).toBool(),
        .createdAt = std::chrono::local_seconds{std::chrono::seconds{query.value(7).toLongLong()}}};
    if (!query.value(2).isNull()) {
        item.dueDate = std::chrono::year{query.value(2).toInt()}
            / std::chrono::month{query.value(3).toUInt()} / std::chrono::day{query.value(4).toUInt()};
        if (!item.dueDate->ok()) {
            throw std::runtime_error{"В SQLite сохранён некорректный срок задачи"};
        }
    }

    return item;
}

}  // namespace

SqliteTaskRepository::SqliteTaskRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteTaskRepository::add(const core::Task& item)
{
    if (item.identifier != 0) {
        throw std::invalid_argument{"Новая задача должна иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO tasks(title, due_year, due_month, due_day, priority, done, created_at) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    bindTask(query, item);
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::optional<core::Task> SqliteTaskRepository::find(std::int64_t identifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, due_year, due_month, due_day, priority, done, created_at "
                                  "FROM tasks WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readTask(query);
}

std::vector<core::Task> SqliteTaskRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, due_year, due_month, due_day, priority, done, created_at "
                                  "FROM tasks"));
    execute(query);
    std::vector<core::Task> result;
    while (next(query)) {
        result.push_back(readTask(query));
    }

    return result;
}

bool SqliteTaskRepository::update(const core::Task& item)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("UPDATE tasks SET title = ?, due_year = ?, due_month = ?, due_day = ?, "
                                  "priority = ?, done = ?, created_at = ? WHERE identifier = ?"));
    bindTask(query, item);
    query.addBindValue(QVariant::fromValue<qlonglong>(item.identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

bool SqliteTaskRepository::remove(std::int64_t identifier)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("DELETE FROM tasks WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

}  // namespace msa::infra

