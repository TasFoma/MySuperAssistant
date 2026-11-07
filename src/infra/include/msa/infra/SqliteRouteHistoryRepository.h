#pragma once

#include "msa/core/RouteHistoryRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra
{

/// @brief SQLite-история расчётов маршрутов, участвующая во внешних транзакциях.
class SqliteRouteHistoryRepository final : public core::RouteHistoryRepository
{
public:
    /// @brief База со всеми sqliteMigrations должна пережить репозиторий.
    explicit SqliteRouteHistoryRepository(SqliteDatabase& database);
    /// @brief Сохраняет полный расчёт; неверные данные — invalid_argument.
    [[nodiscard]] std::int64_t add(const core::RouteCalculation& calculation) override;
    /// @brief Независимые копии без порядка; SQL/повреждение — runtime_error.
    [[nodiscard]] std::vector<core::RouteCalculation> all() const override;

private:
    SqliteDatabase& m_database;
};

} // namespace msa::infra
