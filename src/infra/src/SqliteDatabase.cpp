#include "msa/infra/SqliteDatabase.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <QVariant>

#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>

namespace msa::infra {
namespace {

constexpr int kBaseSchemaVersion = 1;
constexpr int kApplicationIdentifier = 0x4D534131;

[[nodiscard]] std::runtime_error sqlError(const QString& operation, const QSqlError& error)
{
    return std::runtime_error{(operation + QStringLiteral(": ") + error.text()).toStdString()};
}

void validateMigrations(const std::vector<SqliteMigration>& migrations)
{
    std::int64_t expectedVersion = kBaseSchemaVersion + 1;
    for (const auto& migration : migrations) {
        if (migration.version != expectedVersion) {
            throw std::invalid_argument{"Миграции SQLite должны идти подряд начиная с версии 2"};
        }
        for (const auto& statement : migration.statements) {
            if (statement.trimmed().isEmpty()) {
                throw std::invalid_argument{"SQL-оператор миграции не должен быть пустым"};
            }
        }
        ++expectedVersion;
    }
}

}  // namespace

SqliteDatabase::SqliteDatabase(const QString& filePath, const std::vector<SqliteMigration>& migrations)
{
    if (filePath.trimmed().isEmpty()) {
        throw std::invalid_argument{"Укажите путь к файлу SQLite"};
    }
    if (QCoreApplication::instance() == nullptr) {
        throw std::runtime_error{"Создайте QCoreApplication до открытия SQLite"};
    }
    validateMigrations(migrations);
    m_connectionName = QStringLiteral("msa_sqlite_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    try {
        m_database.setDatabaseName(filePath);
        if (!m_database.open()) {
            throw sqlError(QStringLiteral("Не удалось открыть SQLite"), m_database.lastError());
        }
        execute(QStringLiteral("PRAGMA foreign_keys = ON"));
        if (integerValue(QStringLiteral("PRAGMA foreign_keys")) != 1) {
            throw std::runtime_error{"Не удалось включить внешние ключи SQLite"};
        }
        initialize(migrations);
    } catch (...) {
        releaseConnection();
        throw;
    }
}

SqliteDatabase::~SqliteDatabase()
{
    releaseConnection();
}

void SqliteDatabase::releaseConnection()
{
    m_database.close();
    m_database = QSqlDatabase{};
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SqliteDatabase::ensureUsable() const
{
    if (m_database.thread() != QThread::currentThread()) {
        throw std::runtime_error{"SQLite-соединение используется из другого потока"};
    }
    if (m_failed || !m_database.isOpen()) {
        throw std::runtime_error{"SQLite-соединение закрыто или требует повторного открытия после сбоя"};
    }
}

const QSqlDatabase& SqliteDatabase::connection() const
{
    ensureUsable();

    return m_database;
}

int SqliteDatabase::integerValue(const QString& statement) const
{
    QSqlQuery query{connection()};
    if (!query.exec(statement) || !query.next()) {
        throw sqlError(QStringLiteral("Не удалось прочитать состояние SQLite"), query.lastError());
    }
    bool valid = false;
    const auto value = query.value(0).toInt(&valid);
    if (!valid) {
        throw std::runtime_error{"SQLite вернула неверное целочисленное состояние"};
    }

    return value;
}

int SqliteDatabase::schemaVersion() const
{
    return integerValue(QStringLiteral("PRAGMA user_version"));
}

void SqliteDatabase::execute(const QString& statement)
{
    QSqlQuery query{connection()};
    if (!query.exec(statement)) {
        throw sqlError(QStringLiteral("Ошибка выполнения SQL"), query.lastError());
    }
}

void SqliteDatabase::transaction(const std::function<void()>& operation)
{
    ensureUsable();
    if (!operation) {
        throw std::invalid_argument{"Не задана операция транзакции SQLite"};
    }
    if (m_transactionDepth == std::numeric_limits<int>::max()) {
        throw std::overflow_error{"Слишком большая вложенность транзакций SQLite"};
    }
    const auto name = QStringLiteral("msa_transaction_") + QString::number(m_transactionDepth);
    execute(QStringLiteral("SAVEPOINT ") + name);
    ++m_transactionDepth;
    try {
        operation();
        execute(QStringLiteral("RELEASE SAVEPOINT ") + name);
        --m_transactionDepth;
    } catch (...) {
        const auto originalError = std::current_exception();
        --m_transactionDepth;
        try {
            execute(QStringLiteral("ROLLBACK TO SAVEPOINT ") + name);
            execute(QStringLiteral("RELEASE SAVEPOINT ") + name);
        } catch (...) {
            m_failed = true;
            m_database.close();
            std::throw_with_nested(std::runtime_error{"Не удалось откатить SQLite; соединение закрыто"});
        }
        std::rethrow_exception(originalError);
    }
}

void SqliteDatabase::initialize(const std::vector<SqliteMigration>& migrations)
{
    const auto supportedVersion = migrations.empty() ? kBaseSchemaVersion : migrations.back().version;
    transaction([this, &migrations, supportedVersion]() {
        const auto currentVersion = schemaVersion();
        const auto applicationIdentifier = integerValue(QStringLiteral("PRAGMA application_id"));
        if (currentVersion < 0 || currentVersion > supportedVersion) {
            throw std::runtime_error{"Версия схемы SQLite не поддерживается этим приложением"};
        }
        if (currentVersion == 0) {
            const auto objectCount = integerValue(QStringLiteral(
                "SELECT COUNT(*) FROM sqlite_schema WHERE name NOT GLOB 'sqlite_*'"));
            if (applicationIdentifier != 0 || objectCount != 0) {
                throw std::runtime_error{"Файл SQLite содержит неизвестную схему; выберите новую пустую базу"};
            }
            execute(QStringLiteral("PRAGMA application_id = ") + QString::number(kApplicationIdentifier));
            execute(QStringLiteral("PRAGMA user_version = ") + QString::number(kBaseSchemaVersion));
        } else if (applicationIdentifier != kApplicationIdentifier) {
            throw std::runtime_error{"Файл SQLite не является базой MySuperAssistant"};
        }
        for (const auto& migration : migrations) {
            if (migration.version <= currentVersion) {
                continue;
            }
            for (const auto& statement : migration.statements) {
                execute(statement);
            }
            execute(QStringLiteral("PRAGMA user_version = ") + QString::number(migration.version));
        }
    });
}

}  // namespace msa::infra

