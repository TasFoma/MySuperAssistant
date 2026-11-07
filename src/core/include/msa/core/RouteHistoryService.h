#pragma once

#include "msa/core/Clock.h"
#include "msa/core/RouteHistoryRepository.h"

#include <cstddef>

namespace msa::core
{

/// @brief Статистика построений, не фактического передвижения.
struct RouteCalculationStatistics
{
    /// @brief Число расчётов, полученных от маршрутизатора.
    std::size_t routedCount{};
    /// @brief Число приблизительных оценок; не входит в остальные суммы.
    std::size_t approximateCount{};
    /// @brief Сумма рассчитанных метров только маршрутизатора.
    double plannedDistanceMeters{};
    /// @brief Сумма прогнозируемых секунд только маршрутизатора.
    double plannedDurationSeconds{};
};

/// @brief Запись расчётов, последние пять записей и статистика.
class RouteHistoryService
{
public:
    /// @brief Репозиторий и часы должны пережить сервис.
    RouteHistoryService(RouteHistoryRepository& repository, const Clock& clock);
    /// @brief Назначает время из Clock и сохраняет полный расчёт без повторов.
    /// Некорректные данные вызывают invalid_argument до обращения к хранилищу.
    [[nodiscard]] std::int64_t record(RouteCalculation calculation);
    /// @brief Последние пять записей по ID от новых к старым, независимо от перевода часов.
    [[nodiscard]] std::vector<RouteCalculation> recent() const;
    /// @brief Считает прогнозы отдельно от приблизительных оценок; переполнение — исключение.
    [[nodiscard]] RouteCalculationStatistics statistics() const;

private:
    RouteHistoryRepository& m_repository;
    const Clock& m_clock;
};

} // namespace msa::core
