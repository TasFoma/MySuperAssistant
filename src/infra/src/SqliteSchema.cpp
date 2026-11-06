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
                       "chat_identifier INTEGER NOT NULL REFERENCES chats(identifier))")}},
        {.version = 3, .statements = {
            QStringLiteral("CREATE UNIQUE INDEX chat_message_source "
                           "ON chat_messages(identifier, chat_identifier)"),
            QStringLiteral("CREATE TABLE profile_facts ("
                           "identifier INTEGER PRIMARY KEY CHECK(identifier > 0), "
                           "position INTEGER NOT NULL UNIQUE CHECK(position >= 0), "
                           "text TEXT NOT NULL, always_include INTEGER NOT NULL CHECK(always_include IN (0, 1)), "
                           "source_chat INTEGER, source_message INTEGER, source_mood INTEGER, "
                           "replaces_fact INTEGER REFERENCES profile_facts(identifier) DEFERRABLE INITIALLY DEFERRED, "
                           "superseded_by INTEGER REFERENCES profile_facts(identifier) DEFERRABLE INITIALLY DEFERRED, "
                           "CHECK((source_chat IS NULL AND source_message IS NULL AND source_mood IS NULL) OR "
                           "(source_chat IS NOT NULL AND source_message IS NOT NULL)), "
                           "CHECK(source_mood IS NULL OR source_mood > 0), "
                           "FOREIGN KEY(source_message, source_chat) "
                           "REFERENCES chat_messages(identifier, chat_identifier))"),
            QStringLiteral("CREATE TABLE profile_fact_tags ("
                           "fact_identifier INTEGER NOT NULL REFERENCES profile_facts(identifier) ON DELETE CASCADE, "
                           "position INTEGER NOT NULL CHECK(position >= 0), text TEXT NOT NULL, "
                           "PRIMARY KEY(fact_identifier, position))")}},
        {.version = 4, .statements = {
            QStringLiteral("CREATE TABLE greeting_state ("
                           "identifier INTEGER PRIMARY KEY CHECK(identifier = 1), "
                           "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                           "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                           "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31))")}},
        {.version = 5, .statements = {
            QStringLiteral("CREATE TABLE evening_check_in_state ("
                           "identifier INTEGER PRIMARY KEY CHECK(identifier = 1), "
                           "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                           "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                           "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31))"),
            QStringLiteral("CREATE TABLE pending_evening_check_in ("
                           "identifier INTEGER PRIMARY KEY CHECK(identifier = 1), "
                           "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                           "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                           "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31), "
                           "chat_identifier INTEGER NOT NULL CHECK(chat_identifier > 0), "
                           "message_identifier INTEGER NOT NULL CHECK(message_identifier > 0), "
                           "FOREIGN KEY(message_identifier, chat_identifier) "
                           "REFERENCES chat_messages(identifier, chat_identifier))")}},
        {.version = 6, .statements = {
            QStringLiteral("CREATE TABLE event_warning_marks ("
                           "event_identifier INTEGER NOT NULL CHECK(event_identifier > 0), "
                           "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                           "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                           "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31), "
                           "kind TEXT NOT NULL CHECK(kind IN ('BeforeStart', 'DayBefore', 'OnDay')), "
                           "PRIMARY KEY(event_identifier, year, month, day, kind))")}},
        {.version = 7, .statements = {
            QStringLiteral("CREATE TABLE mood_entries ("
                           "identifier INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "recorded_at INTEGER NOT NULL, score INTEGER NOT NULL CHECK(score IN (-1, 0, 1)), "
                           "note TEXT NOT NULL)"),
            // Пересоздаём профиль с внешним ключом, не отключая foreign_keys.
            QStringLiteral("CREATE TABLE profile_facts_new ("
                           "identifier INTEGER PRIMARY KEY CHECK(identifier > 0), "
                           "position INTEGER NOT NULL UNIQUE CHECK(position >= 0), "
                           "text TEXT NOT NULL, always_include INTEGER NOT NULL CHECK(always_include IN (0, 1)), "
                           "source_chat INTEGER, source_message INTEGER, "
                           "source_mood INTEGER REFERENCES mood_entries(identifier), "
                           "replaces_fact INTEGER REFERENCES profile_facts_new(identifier) DEFERRABLE INITIALLY DEFERRED, "
                           "superseded_by INTEGER REFERENCES profile_facts_new(identifier) DEFERRABLE INITIALLY DEFERRED, "
                           "CHECK((source_chat IS NULL AND source_message IS NULL AND source_mood IS NULL) OR "
                           "(source_chat IS NOT NULL AND source_message IS NOT NULL)), "
                           "CHECK(source_mood IS NULL OR source_mood > 0), "
                           "FOREIGN KEY(source_message, source_chat) "
                           "REFERENCES chat_messages(identifier, chat_identifier))"),
            QStringLiteral("INSERT INTO profile_facts_new SELECT * FROM profile_facts"),
            QStringLiteral("CREATE TABLE profile_fact_tags_new ("
                           "fact_identifier INTEGER NOT NULL REFERENCES profile_facts_new(identifier) ON DELETE CASCADE, "
                           "position INTEGER NOT NULL CHECK(position >= 0), text TEXT NOT NULL, "
                           "PRIMARY KEY(fact_identifier, position))"),
            QStringLiteral("INSERT INTO profile_fact_tags_new SELECT * FROM profile_fact_tags"),
            QStringLiteral("DROP TABLE profile_fact_tags"),
            QStringLiteral("DROP TABLE profile_facts"),
            QStringLiteral("ALTER TABLE profile_facts_new RENAME TO profile_facts"),
            QStringLiteral("ALTER TABLE profile_fact_tags_new RENAME TO profile_fact_tags")}},
        {.version = 8, .statements = {
            QStringLiteral("CREATE TABLE tasks ("
                           "identifier INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, "
                           "due_year INTEGER, due_month INTEGER, due_day INTEGER, "
                           "priority INTEGER NOT NULL CHECK(priority IN (0, 1, 2)), "
                           "done INTEGER NOT NULL CHECK(done IN (0, 1)), created_at INTEGER NOT NULL, "
                           "CHECK((due_year IS NULL AND due_month IS NULL AND due_day IS NULL) OR "
                           "(due_year IS NOT NULL AND due_month IS NOT NULL AND due_day IS NOT NULL "
                           "AND due_year BETWEEN -32767 AND 32767 AND due_month BETWEEN 1 AND 12 "
                           "AND due_day BETWEEN 1 AND 31)))")}}};
}

}  // namespace msa::infra

