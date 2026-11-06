#include "msa/infra/SqliteUserProfileRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <map>
#include <optional>
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

[[nodiscard]] QVariant identifierValue(std::optional<std::int64_t> identifier)
{
    return identifier ? QVariant::fromValue<qlonglong>(*identifier) : QVariant{};
}

[[nodiscard]] std::optional<std::int64_t> optionalIdentifier(const QVariant& value)
{
    if (value.isNull()) {
        return std::nullopt;
    }

    return value.toLongLong();
}

}  // namespace

SqliteUserProfileRepository::SqliteUserProfileRepository(SqliteDatabase& database)
    : m_database{database}
{
}

core::UserProfile SqliteUserProfileRepository::load() const
{
    core::UserProfile result;
    // Два чтения образуют один снимок, даже если файл открыт другим соединением.
    m_database.transaction([this, &result]() {
        QSqlQuery query{m_database.connection()};
        prepare(query, QStringLiteral("SELECT identifier, text, always_include, source_chat, source_message, "
                                      "source_mood, replaces_fact, superseded_by "
                                      "FROM profile_facts ORDER BY position"));
        execute(query);
        std::map<std::int64_t, std::size_t> positions;
        while (next(query)) {
            core::UserProfileFact fact{.text = query.value(1).toString().toStdString(),
                .alwaysInclude = query.value(2).toBool(), .identifier = query.value(0).toLongLong(),
                .replacesFactIdentifier = optionalIdentifier(query.value(6)),
                .supersededByFactIdentifier = optionalIdentifier(query.value(7))};
            if (!query.value(3).isNull()) {
                fact.source = core::ProfileFactSource{.chatIdentifier = query.value(3).toLongLong(),
                    .messageIdentifier = query.value(4).toLongLong(),
                    .moodIdentifier = optionalIdentifier(query.value(5))};
            }
            positions.emplace(fact.identifier, result.facts.size());
            result.facts.push_back(fact);
        }
        query.finish();
        prepare(query, QStringLiteral("SELECT fact_identifier, text FROM profile_fact_tags "
                                      "ORDER BY fact_identifier, position"));
        execute(query);
        while (next(query)) {
            result.facts.at(positions.at(query.value(0).toLongLong())).tags.push_back(
                query.value(1).toString().toStdString());
        }
    });

    return result;
}

void SqliteUserProfileRepository::save(const core::UserProfile& profile)
{
    m_database.transaction([this, &profile]() {
        // Отложенные ссылки версий позволяют заменить весь снимок с взаимными ссылками.
        m_database.execute(QStringLiteral("DELETE FROM profile_facts"));
        QSqlQuery facts{m_database.connection()};
        prepare(facts, QStringLiteral("INSERT INTO profile_facts(identifier, position, text, always_include, "
                                      "source_chat, source_message, source_mood, replaces_fact, superseded_by) "
                                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        QSqlQuery tags{m_database.connection()};
        prepare(tags, QStringLiteral("INSERT INTO profile_fact_tags(fact_identifier, position, text) "
                                     "VALUES (?, ?, ?)"));
        qlonglong factPosition = 0;
        for (const auto& fact : profile.facts) {
            facts.bindValue(0, identifierValue(fact.identifier));
            facts.bindValue(1, factPosition++);
            facts.bindValue(2, QString::fromStdString(fact.text));
            facts.bindValue(3, fact.alwaysInclude);
            facts.bindValue(4, identifierValue(fact.source
                ? std::optional{fact.source->chatIdentifier} : std::nullopt));
            facts.bindValue(5, identifierValue(fact.source
                ? std::optional{fact.source->messageIdentifier} : std::nullopt));
            facts.bindValue(6, identifierValue(fact.source ? fact.source->moodIdentifier : std::nullopt));
            facts.bindValue(7, identifierValue(fact.replacesFactIdentifier));
            facts.bindValue(8, identifierValue(fact.supersededByFactIdentifier));
            execute(facts);
            qlonglong tagPosition = 0;
            for (const auto& tag : fact.tags) {
                tags.bindValue(0, identifierValue(fact.identifier));
                tags.bindValue(1, tagPosition++);
                tags.bindValue(2, QString::fromStdString(tag));
                execute(tags);
            }
        }
    });
}

}  // namespace msa::infra

