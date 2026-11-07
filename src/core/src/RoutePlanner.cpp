#include "msa/core/RoutePlanner.h"

#include "msa/core/NavigationService.h"

#include <algorithm>
#include <cmath>

namespace msa::core
{

RoutePlanner::~RoutePlanner() = default;

ExternalServiceResult<RoutePlan> RoutePlanner::route(GeoCoordinates origin, GeoCoordinates destination, TravelMode mode)
{
    if (!validCoordinates(origin) || !validCoordinates(destination) ||
        (mode != TravelMode::Foot && mode != TravelMode::Bike && mode != TravelMode::Driving))
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Укажите корректные координаты и способ передвижения."};
    }
    auto result = requestRoute(origin, destination, mode);
    if (const auto* plan = std::get_if<RoutePlan>(&result))
    {
        if (!std::isfinite(plan->distanceMeters) || plan->distanceMeters < 0 || !std::isfinite(plan->durationSeconds) ||
            plan->durationSeconds < 0 || plan->approximate || plan->steps.empty() ||
            std::ranges::any_of(
                plan->steps,
                [](const RouteStep& step)
                {
                    return step.type.empty() || !std::isfinite(step.distanceMeters) || step.distanceMeters < 0 ||
                           (step.exitNumber && *step.exitNumber <= 0);
                }))
        {
            return ExternalServiceError{ExternalServiceErrorKind::InvalidResponse,
                                        "Сервис маршрутов вернул неполные или неверные данные."};
        }
    }

    return result;
}

} // namespace msa::core
