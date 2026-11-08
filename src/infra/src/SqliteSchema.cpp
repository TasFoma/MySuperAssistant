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
                           "AND due_day BETWEEN 1 AND 31)))")}},
        {.version = 9, .statements = {
            QStringLiteral("CREATE TABLE events ("
                           "identifier INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, "
                           "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                           "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                           "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31), "
                           "time_minutes INTEGER CHECK(time_minutes BETWEEN 0 AND 1439), "
                           "description TEXT NOT NULL, repeat INTEGER NOT NULL CHECK(repeat IN (0, 1, 2, 3)))")}},
        {.version = 10, .statements = {
            QStringLiteral("CREATE TABLE reminders ("
                "identifier INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, "
                "kind TEXT NOT NULL CHECK(kind IN ('Timed', 'Recurring', 'Place')), "
                "due_at INTEGER, interval_minutes INTEGER, place TEXT, latitude REAL, longitude REAL, status INTEGER, "
                "CHECK((latitude IS NULL AND longitude IS NULL) OR "
                "(latitude IS NOT NULL AND longitude IS NOT NULL AND latitude BETWEEN -90 AND 90 "
                "AND longitude BETWEEN -180 AND 180)), "
                "CHECK((kind = 'Timed' AND due_at IS NOT NULL AND interval_minutes IS NULL AND place IS NULL "
                "AND latitude IS NULL AND longitude IS NULL AND status IS NOT NULL AND status IN (0, 1, 2)) OR "
                "(kind = 'Recurring' AND due_at IS NOT NULL AND interval_minutes IS NOT NULL "
                "AND interval_minutes > 0 AND place IS NULL AND latitude IS NULL AND longitude IS NULL AND status IS NULL) OR "
                "(kind = 'Place' AND due_at IS NULL AND interval_minutes IS NULL AND place IS NOT NULL "
                "AND status IS NOT NULL AND status IN (0, 1, 2))))")}},
        {.version = 11, .statements = {
            QStringLiteral("CREATE TABLE habits ("
                "identifier INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, "
                "target REAL NOT NULL CHECK(target > 0 AND target <= 1.7976931348623157e308), "
                "unit TEXT NOT NULL, period INTEGER NOT NULL CHECK(period IN (0, 1)))"),
            QStringLiteral("CREATE TABLE habit_logs ("
                "identifier INTEGER PRIMARY KEY AUTOINCREMENT, "
                "habit_identifier INTEGER NOT NULL REFERENCES habits(identifier) ON DELETE CASCADE, "
                "year INTEGER NOT NULL CHECK(year BETWEEN -32767 AND 32767), "
                "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12), "
                "day INTEGER NOT NULL CHECK(day BETWEEN 1 AND 31), "
                "value REAL NOT NULL CHECK(value > 0 AND value <= 1.7976931348623157e308))"),
            QStringLiteral("CREATE INDEX habit_logs_by_habit ON habit_logs(habit_identifier, identifier)")}},
        {.version = 12, .statements = {
            QStringLiteral("CREATE TABLE route_calculations (identifier INTEGER PRIMARY KEY AUTOINCREMENT, "
                "created_at INTEGER NOT NULL, origin_name TEXT NOT NULL, destination_name TEXT NOT NULL, "
                "origin_latitude REAL NOT NULL CHECK(origin_latitude BETWEEN -90 AND 90), "
                "origin_longitude REAL NOT NULL CHECK(origin_longitude BETWEEN -180 AND 180), "
                "destination_latitude REAL NOT NULL CHECK(destination_latitude BETWEEN -90 AND 90), "
                "destination_longitude REAL NOT NULL CHECK(destination_longitude BETWEEN -180 AND 180), "
                "mode INTEGER NOT NULL CHECK(mode IN (0, 1, 2)), "
                "distance_meters REAL NOT NULL CHECK(distance_meters >= 0 AND distance_meters <= 1.7976931348623157e308), "
                "duration_seconds REAL NOT NULL CHECK(duration_seconds >= 0 AND duration_seconds <= 1.7976931348623157e308), "
                "approximate INTEGER NOT NULL CHECK(approximate IN (0, 1)))"),
            QStringLiteral("CREATE TABLE geocoding_cache (source TEXT NOT NULL, query TEXT NOT NULL, "
                "stored_at INTEGER NOT NULL, PRIMARY KEY(source, query))"),
            QStringLiteral("CREATE TABLE geocoding_cache_places (source TEXT NOT NULL, query TEXT NOT NULL, "
                "position INTEGER NOT NULL CHECK(position >= 0), "
                "latitude REAL NOT NULL CHECK(latitude BETWEEN -90 AND 90), "
                "longitude REAL NOT NULL CHECK(longitude BETWEEN -180 AND 180), name TEXT NOT NULL, "
                "PRIMARY KEY(source, query, position), "
                "FOREIGN KEY(source, query) REFERENCES geocoding_cache(source, query) ON DELETE CASCADE)")}},
        {.version = 13, .statements = {
            QStringLiteral("CREATE TABLE server_state (singleton INTEGER PRIMARY KEY CHECK(singleton = 1), "
                "main_chat_identifier INTEGER NOT NULL REFERENCES chats(identifier))")}}};
}

}  // namespace msa::infra

