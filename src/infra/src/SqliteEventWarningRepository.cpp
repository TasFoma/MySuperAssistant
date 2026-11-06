#include "msa/infra/SqliteEventWarningRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <stdexcept>

namespace msa::infra {
namespace {

[[nodiscard]] QString kindText(core::EventWarningKind kind)
{
    switch (kind) {
    case core::EventWarningKind::BeforeStart:
        return QStringLiteral("BeforeStart");
    case core::EventWarningKind::DayBefore:
        return QStringLiteral("DayBefore");
    case core::EventWarningKind::OnDay:
        return QStringLiteral("OnDay");
    }
    throw std::invalid_argument{"Неизвестный вид предупреждения о событии"};
}

}  // namespace

SqliteEventWarningRepository::SqliteEventWarningRepository(SqliteDatabase& database)
    : m_database{database}
{
}

bool SqliteEventWarningRepository::markIfNew(std::int64_t identifier,
    std::chrono::year_month_day occurrenceDate, core::EventWarningKind kind)
{
    if (identifier <= 0 || !occurrenceDate.ok()) {
        throw std::invalid_argument{"Укажите положительный ID события и корректную дату вхождения"};
    }
    const auto storedKind = kindText(kind);
    QSqlQuery query{m_database.connection()};
    // Обрабатываем только конфликт ключа; остальные ошибки не маскируются повтором.
    if (!query.prepare(QStringLiteral("INSERT INTO event_warning_marks "
        "(event_identifier, year, month, day, kind) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(event_identifier, year, month, day, kind) DO NOTHING"))) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    query.addBindValue(static_cast<int>(occurrenceDate.year()));
    query.addBindValue(static_cast<unsigned>(occurrenceDate.month()));
    query.addBindValue(static_cast<unsigned>(occurrenceDate.day()));
    query.addBindValue(storedKind);
    if (!query.exec()) {
        throw std::runtime_error{query.lastError().text().toStdString()};
    }

    return query.numRowsAffected() != 0;
}

}  // namespace msa::infra

