#include "msa/infra/SqliteChatRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <stdexcept>
#include <string>

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

[[nodiscard]] core::Chat readChat(const QSqlQuery& query)
{
    return {.identifier = query.value(0).toLongLong(),
        .title = query.value(1).toString().toStdString(),
        .createdAt = std::chrono::local_seconds{std::chrono::seconds{query.value(2).toLongLong()}}};
}

[[nodiscard]] QString roleText(core::MessageRole role)
{
    switch (role) {
    case core::MessageRole::User:
        return QStringLiteral("User");
    case core::MessageRole::Assistant:
        return QStringLiteral("Assistant");
    }
    throw std::invalid_argument{"Неизвестная роль сообщения чата"};
}

[[nodiscard]] core::MessageRole readRole(const QString& role)
{
    if (role == QStringLiteral("User")) {
        return core::MessageRole::User;
    }
    if (role == QStringLiteral("Assistant")) {
        return core::MessageRole::Assistant;
    }
    throw std::runtime_error{"В SQLite сохранена неизвестная роль сообщения"};
}

}  // namespace

SqliteChatRepository::SqliteChatRepository(SqliteDatabase& database)
    : m_database{database}
{
}

std::int64_t SqliteChatRepository::addChat(const core::Chat& chat)
{
    if (chat.identifier != 0) {
        throw std::invalid_argument{"Новый чат должен иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO chats(title, created_at) VALUES (?, ?)"));
    query.addBindValue(QString::fromStdString(chat.title));
    query.addBindValue(QVariant::fromValue<qlonglong>(chat.createdAt.time_since_epoch().count()));
    execute(query);

    return query.lastInsertId().toLongLong();
}

std::optional<core::Chat> SqliteChatRepository::findChat(std::int64_t identifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, created_at FROM chats WHERE identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return readChat(query);
}

std::vector<core::Chat> SqliteChatRepository::chats() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, title, created_at FROM chats"));
    execute(query);
    std::vector<core::Chat> result;
    while (next(query)) {
        result.push_back(readChat(query));
    }

    return result;
}

std::optional<std::int64_t> SqliteChatRepository::addMessage(const core::ChatMessage& message)
{
    if (message.identifier != 0) {
        throw std::invalid_argument{"Новое сообщение должно иметь нулевой ID"};
    }
    QSqlQuery query{m_database.connection()};
    // Проверка существования и вставка составляют один SQL-оператор.
    prepare(query, QStringLiteral("INSERT INTO chat_messages(chat_identifier, role, text, created_at) "
                                  "SELECT identifier, ?, ?, ? FROM chats WHERE identifier = ?"));
    query.addBindValue(roleText(message.role));
    query.addBindValue(QString::fromStdString(message.text));
    query.addBindValue(QVariant::fromValue<qlonglong>(message.createdAt.time_since_epoch().count()));
    query.addBindValue(QVariant::fromValue<qlonglong>(message.chatIdentifier));
    execute(query);
    if (query.numRowsAffected() == 0) {
        return std::nullopt;
    }

    return query.lastInsertId().toLongLong();
}

std::vector<core::ChatMessage> SqliteChatRepository::messages(std::int64_t chatIdentifier) const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT identifier, chat_identifier, role, text, created_at "
                                  "FROM chat_messages WHERE chat_identifier = ?"));
    query.addBindValue(QVariant::fromValue<qlonglong>(chatIdentifier));
    execute(query);
    std::vector<core::ChatMessage> result;
    while (next(query)) {
        result.push_back({.identifier = query.value(0).toLongLong(),
            .chatIdentifier = query.value(1).toLongLong(),
            .role = readRole(query.value(2).toString()),
            .text = query.value(3).toString().toStdString(),
            .createdAt = std::chrono::local_seconds{std::chrono::seconds{query.value(4).toLongLong()}}});
    }

    return result;
}

std::optional<std::int64_t> SqliteChatRepository::currentChatId() const
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("SELECT chat_identifier FROM chat_selection WHERE identifier = 1"));
    execute(query);
    if (!next(query)) {
        return std::nullopt;
    }

    return query.value(0).toLongLong();
}

bool SqliteChatRepository::saveCurrentChatId(std::int64_t identifier)
{
    QSqlQuery query{m_database.connection()};
    prepare(query, QStringLiteral("INSERT INTO chat_selection(identifier, chat_identifier) "
                                  "SELECT 1, identifier FROM chats WHERE identifier = ? "
                                  "ON CONFLICT(identifier) DO UPDATE "
                                  "SET chat_identifier = excluded.chat_identifier"));
    query.addBindValue(QVariant::fromValue<qlonglong>(identifier));
    execute(query);

    return query.numRowsAffected() != 0;
}

}  // namespace msa::infra

