#include "msa/infra/SqliteSchema.h"

#include <QString>

namespace msa::infra {

std::vector<SqliteMigration> sqliteMigrations()
{
    return {{.version = 2, .statements = {
        QStringLiteral("CREATE TABLE chats ("
                       "identifier INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "title TEXT NOT NULL, created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE chat_messages ("
                       "identifier INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "chat_identifier INTEGER NOT NULL REFERENCES chats(identifier), "
                       "role TEXT NOT NULL CHECK(role IN ('User', 'Assistant')), "
                       "text TEXT NOT NULL, created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX chat_messages_by_chat "
                       "ON chat_messages(chat_identifier, identifier)"),
        QStringLiteral("CREATE TABLE chat_selection ("
                       "identifier INTEGER PRIMARY KEY CHECK(identifier = 1), "
                       "chat_identifier INTEGER NOT NULL REFERENCES chats(identifier))")}}};
}

}  // namespace msa::infra

