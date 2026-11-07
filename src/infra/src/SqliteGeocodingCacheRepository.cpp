#include "msa/infra/SqliteGeocodingCacheRepository.h"

#include "msa/core/NavigationService.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

void execute(QSqlQuery& query, const QString& statement, const QVariantList& values)
{
    if (!query.prepare(statement))
    {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    for (const auto& value : values)
    {
        query.addBindValue(value);
    }
    if (!query.exec())
    {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
}

void validateKey(const std::string& source, const std::string& query)
{
    if (QString::fromStdString(source).trimmed().isEmpty() || QString::fromStdString(query).trimmed().isEmpty())
    {
        throw std::invalid_argument{"Укажите поставщика и запрос геокодинга."};
    }
}

} // namespace

SqliteGeocodingCacheRepository::SqliteGeocodingCacheRepository(SqliteDatabase& database) : m_database{database} {}

std::optional<core::GeocodingCacheEntry> SqliteGeocodingCacheRepository::find(const std::string& source,
                                                                              const std::string& query) const
{
    validateKey(source, query);
    const QVariantList key{QString::fromStdString(source), QString::fromStdString(query)};
    QSqlQuery header{m_database.connection()};
    execute(header, QStringLiteral("SELECT stored_at FROM geocoding_cache WHERE source = ? AND query = ?"), key);
    if (!header.next())
    {
        if (header.lastError().isValid())
        {
            throw std::runtime_error{header.lastError().text().toStdString()};
        }
        return std::nullopt;
    }
    core::GeocodingCacheEntry entry{std::chrono::local_seconds{std::chrono::seconds{header.value(0).toLongLong()}}};
    QSqlQuery places{m_database.connection()};
    execute(places,
            QStringLiteral("SELECT latitude, longitude, name FROM geocoding_cache_places "
                           "WHERE source = ? AND query = ? ORDER BY position"),
            key);
    while (places.next())
    {
        core::GeocodedPlace place{{places.value(0).toDouble(), places.value(1).toDouble()},
                                  places.value(2).toString().toStdString()};
        if (!core::validCoordinates(place.coordinates) || QString::fromStdString(place.name).trimmed().isEmpty())
        {
            throw std::runtime_error{"Повреждён кандидат в кэше геокодинга."};
        }
        entry.places.push_back(std::move(place));
    }
    if (places.lastError().isValid())
    {
        throw std::runtime_error{places.lastError().text().toStdString()};
    }

    return entry;
}

void SqliteGeocodingCacheRepository::save(const std::string& source, const std::string& query,
                                          const core::GeocodingCacheEntry& entry)
{
    validateKey(source, query);
    if (std::ranges::any_of(entry.places,
                            [](const core::GeocodedPlace& place)
                            {
                                return !core::validCoordinates(place.coordinates) ||
                                       QString::fromStdString(place.name).trimmed().isEmpty();
                            }))
    {
        throw std::invalid_argument{"Некорректные кандидаты геокодинга."};
    }
    m_database.transaction(
        [this, &source, &query, &entry]()
        {
            const auto sourceText = QString::fromStdString(source);
            const auto queryText = QString::fromStdString(query);
            QSqlQuery statement{m_database.connection()};
            execute(statement,
                    QStringLiteral("INSERT INTO geocoding_cache(source, query, stored_at) VALUES (?, ?, ?) "
                                   "ON CONFLICT(source, query) DO UPDATE SET stored_at = excluded.stored_at"),
                    {sourceText, queryText, QVariant::fromValue<qlonglong>(entry.storedAt.time_since_epoch().count())});
            execute(statement, QStringLiteral("DELETE FROM geocoding_cache_places WHERE source = ? AND query = ?"),
                    {sourceText, queryText});
            qlonglong position = 0;
            for (const auto& place : entry.places)
            {
                execute(statement,
                        QStringLiteral(
                            "INSERT INTO geocoding_cache_places(source, query, position, latitude, longitude, name) "
                            "VALUES (?, ?, ?, ?, ?, ?)"),
                        {sourceText, queryText, position++, place.coordinates.latitude, place.coordinates.longitude,
                         QString::fromStdString(place.name)});
            }
        });
}

} // namespace msa::infra
