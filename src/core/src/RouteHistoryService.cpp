#include "msa/core/RouteHistoryService.h"

#include "msa/core/detail/Utf8Text.h"
#include "msa/core/NavigationService.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <utility>

namespace msa::core
{

RouteHistoryService::RouteHistoryService(RouteHistoryRepository& repository, const Clock& clock) :
    m_repository{repository}, m_clock{clock}
{
}

std::int64_t RouteHistoryService::record(RouteCalculation calculation)
{
    if (calculation.identifier != 0 || text::trim(calculation.originName).empty() ||
        text::trim(calculation.destinationName).empty() || !validCoordinates(calculation.origin) ||
        !validCoordinates(calculation.destination) ||
        (calculation.mode != TravelMode::Foot && calculation.mode != TravelMode::Bike &&
         calculation.mode != TravelMode::Driving) ||
        !std::isfinite(calculation.distanceMeters) || calculation.distanceMeters < 0 ||
        !std::isfinite(calculation.durationSeconds) || calculation.durationSeconds < 0)
    {
        throw std::invalid_argument{"Некорректные данные расчёта маршрута."};
    }
    calculation.createdAt = m_clock.now();

    return m_repository.add(calculation);
}

std::vector<RouteCalculation> RouteHistoryService::recent() const
{
    constexpr std::size_t kRecentCount = 5;
    auto calculations = m_repository.all();
    std::ranges::sort(calculations, std::greater<>{}, &RouteCalculation::identifier);
    if (calculations.size() > kRecentCount)
    {
        calculations.resize(kRecentCount);
    }

    return calculations;
}

RouteCalculationStatistics RouteHistoryService::statistics() const
{
    RouteCalculationStatistics result;
    for (const auto& calculation : m_repository.all())
    {
        if (calculation.approximate)
        {
            ++result.approximateCount;
            continue;
        }
        ++result.routedCount;
        result.plannedDistanceMeters += calculation.distanceMeters;
        result.plannedDurationSeconds += calculation.durationSeconds;
        if (!std::isfinite(result.plannedDistanceMeters) || !std::isfinite(result.plannedDurationSeconds))
        {
            throw std::overflow_error{"Сумма прогнозов маршрутов слишком велика."};
        }
    }

    return result;
}

} // namespace msa::core
