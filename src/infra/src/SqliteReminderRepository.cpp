#include "msa/infra/SqliteReminderRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace msa::infra {
namespace {

constexpr double kMaxLatitude = 90.0;
constexpr double kMaxLongitude = 180.0;

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

[[nodiscard]] std::int64_t identifierOf(const core::Reminder& reminder)
{
    return std::visit([](const auto& item) { return item.identifier; }, reminder);
}

[[nodiscard]] bool validFields(const core::Reminder& reminder)
{
    return std::visit([](const auto& item) {
        using ReminderType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ReminderType, core::RecurringReminder>) {
            return item.interval > std::chrono::minutes::zero();
        } else {
            const auto validStatus = item.status == core::ReminderStatus::Pending
                || item.status == core::ReminderStatus::Fired || item.status == core::ReminderStatus::Missed;
            if constexpr (std::is_same_v<ReminderType, core::PlaceReminder>) {
                return validStatus && (!item.coordinates
                    || (std::isfinite(item.coordinates->latitude) && std::isfinite(item.coordinates->longitude)
                        && std::abs(item.coordinates->latitude) <= kMaxLatitude
                        && std::abs(item.coordinates->longitude) <= kMaxLongitude));
            } else {
                return validStatus;
            }
        }
    }, reminder);
}

void bindReminder(QSqlQuery& query, const core::Reminder& reminder)
{
    if (!validFields(reminder)) {
        throw std::invalid_argument{"Некорректные статус, интервал или координаты напоминания"};
    }
    // При смене вида все неиспользуемые поля явно сбрасываются в NULL.
    query.bindValue(QStringLiteral(":due"), QVariant{});
    query.bindValue(QStringLiteral(":interval"), QVariant{});
    query.bindValue(QStringLiteral(":place"), QVariant{});
    query.bindValue(QStringLiteral(":latitude"), QVariant{});
    query.bindValue(QStringLiteral(":longitude"), QVariant{});
    query.bindValue(QStringLiteral(":status"), QVariant{});
    std::visit([&query](const auto& item) {
        using ReminderType = std::decay_t<decltype(item)>;
        query.bindValue(QStringLiteral(":title"), QString::fromStdString(item.title));
        if constexpr (std::is_same_v<ReminderType, core::PlaceReminder>) {
            query.bindValue(QStringLiteral(":kind"), QStringLiteral("Place"));
            query.bindValue(QStringLiteral(":place"), QString::fromStdString(item.place));
            if (item.coordinates) {
                query.bindValue(QStringLiteral(":latitude"), item.coordinates->latitude);
                query.bindValue(QStringLiteral(":longitude"), item.coordinates->longitude);
            }
        } else {
            query.bindValue(QStringLiteral(":due"),
                QVariant::fromValue<qlonglong>(item.dueAt.time_since_epoch().count()));
            if constexpr (std::is_same_v<ReminderType, core::RecurringReminder>) {
                query.bindValue(QStringLiteral(":kind"), QStringLiteral("Recurring"));
                query.bindValue(QStringLiteral(":interval"), QVariant::fromValue<qlonglong>(item.interval.count()));
            } else {
                query.bindValue(QStringLiteral(":kind"), QStringLiteral("Timed"));
            }
        }
        if constexpr (!std::is_same_v<ReminderType, core::RecurringReminder>) {
            query.bindValue(QStringLiteral(":status"), static_cast<int>(item.status));
        }
    }, reminder);
}

[[nodiscard]] core::ReminderStatus readStatus(const QSqlQuery& query)
{
    const auto status = query.value(8).toInt();
    if (status < static_cast<int>(core::ReminderStatus::Pending)
        || status > static_cast<int>(core::ReminderStatus::Missed)) {
        throw std::runtime_error{"В SQLite сохранён неизвестный статус напоминания"};
    }

    return static_cast<core::ReminderStatus>(status);
}

[[nodiscard]] core::Reminder readReminder(const QSqlQuery& query)
{
    const auto identifier = query.value(0).toLongLong();
    const auto title = query.value(1).toString().toStdString();
    const auto kind = query.value(2).toString();
    core::Reminder result;
    if (kind == QStringLiteral("Timed")) {
        result = core::TimedReminder{.identifier = identifier, .title = title,
            .dueAt = std::chrono::local_seconds{std::chrono::seconds{query.value(3).toLongLong()}},
            .status = readStatus(query)};
    } else if (kind == QStringLiteral("Recurring")) {
        result = core::RecurringReminder{.identifier = identifier, .title = title,
            .dueAt = std::chrono::local_seconds{std::chrono::seconds{query.value(3).toLongLong()}},
            .interval = std::chrono::minutes{query.value(4).toLongLong()}};
    } else if (kind == QStringLiteral("Place")) {
        core::PlaceReminder item{.identifier = identifier, .title = title,
            .place = query.value(5).toString().toStdString(), .status = readStatus(query)};
        if (!query.value(6).isNull()) {
            item.coordinates = core::GeoCoordinates{.latitude = query.value(6).toDouble(),
                .longitude = query.value(7).toDouble()};
        }
        result = item;
    } else {
        throw std::runtime_error{"В SQLite сохранён неизвестный вид напоминания"};
    }
    if (!validFields(result)) {
        throw std::runtime_error{"В SQLite сохранены некорректные поля напоминания"};
    }

    return result;
}

}  // namespace

SqliteReminderRepository::SqliteReminderRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteReminderRepository::add(const core::Reminder& item)
{
    if (identifierOf(item) != 0) {
        throw std::invalid_argument{"Новое напоминание должно иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO reminders(title, kind, due_at, interval_minutes, place, latitude, "
        "longitude, status) VALUES (:title, :kind, :due, :interval, :place, :latitude, :longitude, :status)"));
    bindReminder(query, item);
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::optional<core::Reminder> SqliteReminderRepository::find(std::int64_t identifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, kind, due_at, interval_minutes, place, latitude, "
        "longitude, status FROM reminders WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readReminder(query);
}

std::vector<core::Reminder> SqliteReminderRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, kind, due_at, interval_minutes, place, latitude, "
        "longitude, status FROM reminders"));
    execute(query);
    std::vector<core::Reminder> result;
    while (next(query)) {
        result.push_back(readReminder(query));
    }

    return result;
}

bool SqliteReminderRepository::update(const core::Reminder& item)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("UPDATE reminders SET title = :title, kind = :kind, due_at = :due, "
        "interval_minutes = :interval, place = :place, latitude = :latitude, longitude = :longitude, "
        "status = :status WHERE identifier = :identifier"));
    bindReminder(query, item);
    query.bindValue(QStringLiteral(":identifier"), QVariant::fromValue<qlonglong>(identifierOf(item)));
    execute(query);

    return query.numRowsAffected() != 0;
}

bool SqliteReminderRepository::remove(std::int64_t identifier)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("DELETE FROM reminders WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

}  // namespace msa::infra

