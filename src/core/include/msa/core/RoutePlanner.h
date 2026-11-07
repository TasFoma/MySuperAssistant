#pragma once

#include "msa/core/Command.h"
#include "msa/core/ExternalServiceResult.h"
#include "msa/core/GeoCoordinates.h"

#include <optional>
#include <string>
#include <vector>

namespace msa::core
{

/// @brief Манёвр OSRM без Qt и JSON; неизвестные типы сохраняются как строки.
struct RouteStep
{
    /// @brief Тип манёвра: depart, turn, arrive и другие.
    std::string type{};
    /// @brief Направление: left, right и другие.
    std::string modifier{};
    /// @brief Название улицы, если известно.
    std::string street{};
    /// @brief Расстояние до следующего манёвра в метрах.
    double distanceMeters{};
    /// @brief Номер съезда, если поставщик его указал.
    std::optional<int> exitNumber{};
};

/// @brief Полный маршрут или явно обозначенная приблизительная оценка.
struct RoutePlan
{
    /// @brief Расстояние в метрах.
    double distanceMeters{};
    /// @brief Прогноз времени в секундах; не измерение реальной поездки.
    double durationSeconds{};
    /// @brief Пошаговые манёвры; для приблизительной оценки отсутствуют.
    std::vector<RouteStep> steps{};
    /// @brief true только для локальной оценки при недоступности сервиса.
    bool approximate{false};
};

/// @brief Получение маршрута по явным координатам.
class RoutePlanner
{
public:
    /// @brief Освобождает реализацию.
    virtual ~RoutePlanner();
    /// @brief Проверяет координаты/режим и полный ответ; не выполняет повторов.
    [[nodiscard]] ExternalServiceResult<RoutePlan> route(GeoCoordinates origin, GeoCoordinates destination,
                                                         TravelMode mode);

protected:
    /// @brief Один запрос маршрута после проверки входных данных.
    [[nodiscard]] virtual ExternalServiceResult<RoutePlan>
    requestRoute(GeoCoordinates origin, GeoCoordinates destination, TravelMode mode) = 0;
};

} // namespace msa::core
