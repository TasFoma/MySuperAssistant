#pragma once

#include <QStringList>

namespace msa::infra {

/// @brief Один переход схемы: версия и упорядоченные отдельные SQL-операторы.
struct SqliteMigration {
    /// @brief Целевая версия; дополнительные миграции идут подряд начиная с 2.
    int version{0};
    /// @brief Доверенные SQL-константы программы; один оператор на элемент.
    /// Управление транзакцией, user_version и application_id выполняет SqliteDatabase.
    QStringList statements{};
};

}  // namespace msa::infra

