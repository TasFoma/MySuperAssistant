#pragma once

#include "msa/infra/SqliteMigration.h"

#include <vector>

namespace msa::infra {

/// @brief Полный список предметных миграций для открытия базы приложения.
/// Передаётся SqliteDatabase при каждом открытии; версия 12 — история расчётов и кэш геокодинга.
[[nodiscard]] std::vector<SqliteMigration> sqliteMigrations();

}  // namespace msa::infra

