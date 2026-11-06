#pragma once

#include "msa/infra/SqliteMigration.h"

#include <vector>

namespace msa::infra {

/// @brief Полный список предметных миграций для открытия базы приложения.
/// Передаётся SqliteDatabase при каждом открытии; текущая версия — 2 (чаты).
[[nodiscard]] std::vector<SqliteMigration> sqliteMigrations();

}  // namespace msa::infra

