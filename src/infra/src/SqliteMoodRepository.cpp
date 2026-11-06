#include "msa/infra/SqliteMoodRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <stdexcept>

namespace msa::infra {
namespace {

[[nodiscard]] bool validScore(int score)
{
    return score == static_cast<int>(core::MoodScore::Negative)
        || score == static_cast<int>(core::MoodScore::Neutral)
        || score == static_cast<int>(core::MoodScore::Positive);
}

}  // namespace

SqliteMoodRepository::SqliteMoodRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteMoodRepository::add(const core::MoodEntry& entry)
{
    const auto score = static_cast<int>(entry.score);
    if (entry.identifier != 0 || !validScore(score)) {
        throw std::invalid_argument{"Новая запись дневника должна иметь нулевой ID и допустимую оценку"};
    }
    QSqlQuery query{m_database.connection()};
    if (!query.prepare(QStringLiteral("INSERT INTO mood_entries(recorded_at, score, note) VALUES (?, ?, ?)"))) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    query.addBindValue(QVariant::fromValue<qlonglong>(entry.recordedAt.time_since_epoch().count()));
    query.addBindValue(score);
    query.addBindValue(QString::fromStdString(entry.note));
    if (!query.exec()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }

    return query.lastInsertId().toLongLong();
}

std::vector<core::MoodEntry> SqliteMoodRepository::all() const
{
    QSqlQuery query{m_database.connection()};
    if (!query.exec(QStringLiteral("SELECT identifier, recorded_at, score, note FROM mood_entries"))) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    std::vector<core::MoodEntry> result;
    while (query.next()) {
        const auto score = query.value(2).toInt();
        if (!validScore(score)) {
            throw std::runtime_error{"В SQLite сохранена неизвестная оценка настроения"};
        }
        result.push_back({.identifier = query.value(0).toLongLong(),
            .recordedAt = std::chrono::local_seconds{std::chrono::seconds{query.value(1).toLongLong()}},
            .score = static_cast<core::MoodScore>(score), .note = query.value(3).toString().toStdString()});
    }
    if (query.lastError().isValid()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }

    return result;
}

}  // namespace msa::infra

