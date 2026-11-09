#pragma once

#include "msa/core/RouteHistoryRepository.h"

namespace msa::core
{

/// @brief Тестовая история расчётов без файлов.
class InMemoryRouteHistoryRepository final : public RouteHistoryRepository
{
public:
    /// @brief Назначает последовательный ID и сохраняет копию.
    [[nodiscard]] std::int64_t add(const RouteCalculation& calculation) override;
    /// @brief Возвращает независимые копии.
    [[nodiscard]] std::vector<RouteCalculation> all() const override;
    /// @brief Имитирует отказ записи для проверок связки.
    bool failAdd{false};

private:
    std::vector<RouteCalculation> m_items{};
    std::int64_t m_nextIdentifier{1};
};

} // namespace msa::core
