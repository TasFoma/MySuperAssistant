#pragma once

#include "msa/core/TransactionRunner.h"
#include "msa/infra/SqliteMigration.h"

#include <QSqlDatabase>
#include <QString>

#include <functional>
#include <vector>

namespace msa::infra {

/// @brief Владеет именованным SQLite-соединением, версией схемы и транзакциями.
/// Нужен живой QCoreApplication; объект, запросы и репозитории используются в одном потоке.
class SqliteDatabase final : public core::TransactionRunner {
public:
    /// @brief Открывает файл и атомарно применяет недостающие миграции.
    /// Базовая версия 1 помечает принадлежность MySuperAssistant без предметных таблиц.
    /// Путь задаёт вызывающий код; пустой путь отклоняется, каталоги не создаются.
    /// Чужая/неизвестная более новая схема отклоняется без её изменения.
    /// Ошибки ввода — invalid_argument, SQL/файла — runtime_error без Qt-исключений.
    explicit SqliteDatabase(const QString& filePath,
        const std::vector<SqliteMigration>& migrations = {});
    /// @brief Закрывает и удаляет именованное соединение; все внешние запросы уже уничтожены.
    ~SqliteDatabase() override;

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) = delete;
    SqliteDatabase& operator=(SqliteDatabase&&) = delete;

    /// @brief Невладеющий доступ для QSqlQuery; копии соединения не должны переживать владельца.
    /// Не закрывать соединение, не менять PRAGMA схемы и не управлять транзакциями вручную.
    [[nodiscard]] const QSqlDatabase& connection() const;
    /// @brief Читает текущую целую user_version.
    [[nodiscard]] int schemaVersion() const;
    /// @brief Выполняет один SQL-оператор; неверный SQL бросает runtime_error.
    /// Пользовательские значения будущие репозитории передают через bindValue.
    void execute(const QString& statement);
    /// @brief Выполняет callback один раз; успех фиксирует, исключение откатывает и пробрасывает.
    /// Вложенные вызовы используют SAVEPOINT. Внутренний успех не фиксирует внешнюю операцию.
    /// Callback сообщает ошибку исключением, возвращаемый статус не анализируется.
    /// При ошибке отката соединение закрывается, дальнейшее использование отклоняется.
    void transaction(const std::function<void()>& operation) override;

private:
    void initialize(const std::vector<SqliteMigration>& migrations);
    void ensureUsable() const;
    void releaseConnection();
    [[nodiscard]] int integerValue(const QString& statement) const;

    QString m_connectionName{};
    QSqlDatabase m_database{};
    int m_transactionDepth{0};
    bool m_failed{false};
};

}  // namespace msa::infra

