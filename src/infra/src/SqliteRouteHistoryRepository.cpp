#include "msa/infra/SqliteRouteHistoryRepository.h"

#include "msa/core/NavigationService.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

void prepare(QSqlQuery& query, const QString& statement)
{
    if (!query.prepare(statement))
    {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

void execute(QSqlQuery& query)
{
    if (!query.exec())
    {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

[[nodiscard]] bool valid(const core::RouteCalculation& item)
{
    return !QString::fromStdString(item.originName).trimmed().isEmpty() &&
           !QString::fromStdString(item.destinationName).trimmed().isEmpty() && core::validCoordinates(item.origin) &&
           core::validCoordinates(item.destination) &&
           (item.mode == core::TravelMode::Foot || item.mode == core::TravelMode::Bike ||
            item.mode == core::TravelMode::Driving) &&
           std::isfinite(item.distanceMeters) && item.distanceMeters >= 0 && std::isfinite(item.durationSeconds) &&
           item.durationSeconds >= 0;
}

} // namespace

SqliteRouteHistoryRepository::SqliteRouteHistoryRepository(SqliteDatabase& database) : m_database{database} {}

std::int64_t SqliteRouteHistoryRepository::add(const core::RouteCalculation& item)
{
    if (item.identifier != 0 || !valid(item))
    {
        throw std::invalid_argument{"Некорректный расчёт маршрута."};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query,
            QStringLiteral("INSERT INTO route_calculations(created_at, origin_name, destination_name, "
                           "origin_latitude, origin_longitude, destination_latitude, destination_longitude, mode, "
                           "distance_meters, duration_seconds, approximate) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(QVariant::fromValue<qlonglong>(item.createdAt.time_since_epoch().count()));
    query.addBindValue(QString::fromStdString(item.originName));
    query.addBindValue(QString::fromStdString(item.destinationName));
    query.addBindValue(item.origin.latitude);
    query.addBindValue(item.origin.longitude);
    query.addBindValue(item.destination.latitude);
    query.addBindValue(item.destination.longitude);
    query.addBindValue(static_cast<int>(item.mode));
    query.addBindValue(item.distanceMeters);
    query.addBindValue(item.durationSeconds);
    query.addBindValue(item.approximate);
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::vector<core::RouteCalculation> SqliteRouteHistoryRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query,
            QStringLiteral("SELECT identifier, created_at, origin_name, destination_name, "
                           "origin_latitude, origin_longitude, destination_latitude, destination_longitude, mode, "
                           "distance_meters, duration_seconds, approximate FROM route_calculations"));
    execute(query);
    std::vector<core::RouteCalculation> result;
    while (query.next())
    {
        const auto mode = query.value(8).toInt();
        const auto approximate = query.value(11).toInt();
        if (mode < static_cast<int>(core::TravelMode::Foot) || mode > static_cast<int>(core::TravelMode::Bike) ||
            (approximate != 0 && approximate != 1))
        {
            throw std::runtime_error{"Неверный вид сохранённого расчёта."};
        }
        core::RouteCalculation item{.identifier = query.value(0).toLongLong(),
                                    .createdAt =
                                        std::chrono::local_seconds{std::chrono::seconds{query.value(1).toLongLong()}},
                                    .originName = query.value(2).toString().toStdString(),
                                    .destinationName = query.value(3).toString().toStdString(),
                                    .origin = {query.value(4).toDouble(), query.value(5).toDouble()},
                                    .destination = {query.value(6).toDouble(), query.value(7).toDouble()},
                                    .mode = static_cast<core::TravelMode>(mode),
                                    .distanceMeters = query.value(9).toDouble(),
                                    .durationSeconds = query.value(10).toDouble(),
                                    .approximate = approximate != 0};
        if (item.identifier <= 0 || !valid(item))
        {
            throw std::runtime_error{"Повреждён сохранённый расчёт маршрута."};
        }
        result.push_back(std::move(item));
    }
    if (query.lastError().isValid())
    {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }

    return result;
}

} // namespace msa::infra
