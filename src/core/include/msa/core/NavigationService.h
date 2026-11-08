#pragma once

#include "msa/core/Geocoder.h"
#include "msa/core/RouteHistoryService.h"
#include "msa/core/RoutePlanner.h"
#include "msa/core/WeatherProvider.h"

#include <string>

namespace msa::core
{

/// @brief Расчёт без записи истории: позволяет атомарно сохранить его вместе с командой чата.
struct PreparedRoute {
    RoutePlan plan;
    RouteCalculation calculation;
};

/// @brief Проверяет диапазоны и конечность координат, включая допустимые нули.
[[nodiscard]] bool validCoordinates(GeoCoordinates coordinates);
/// @brief Расстояние по сфере в метрах; аргументы должны быть корректными.
[[nodiscard]] double geographicDistance(GeoCoordinates origin, GeoCoordinates destination);
/// @brief Русский текст маршрута с предупреждением для приблизительной оценки.
[[nodiscard]] std::string routeText(const RoutePlan& plan, TravelMode mode, const std::string& origin,
                                    const std::string& destination);
/// @brief Русская сводка проверенного прогноза; неизвестный код не выдаётся за ясную погоду.
[[nodiscard]] std::string weatherText(const WeatherForecast& forecast);

/// @brief Выбор ближайшего места, построение и сохранение расчёта маршрута.
class NavigationService
{
public:
    /// @brief Зависимости должны пережить сервис.
    NavigationService(Geocoder& geocoder, RoutePlanner& planner, RouteHistoryService& history);
    /// @brief Маршрут от явной точки до ближайшего к ней кандидата места.
    [[nodiscard]] ExternalServiceResult<RoutePlan> routeFrom(GeoCoordinates origin, const std::string& destination,
                                                             TravelMode mode);
    /// @brief Оба названия ищутся относительно явной опорной точки пользователя.
    [[nodiscard]] ExternalServiceResult<RoutePlan>
    routeBetween(const std::string& origin, const std::string& destination, GeoCoordinates reference, TravelMode mode);
    /// @brief Выполняет внешний поиск/маршрутизацию без записи истории и без транзакции.
    [[nodiscard]] ExternalServiceResult<PreparedRoute> prepareFrom(
        GeoCoordinates origin, const std::string& destination, TravelMode mode);

private:
    [[nodiscard]] ExternalServiceResult<GeoCoordinates> locate(const std::string& place, GeoCoordinates reference);
    [[nodiscard]] ExternalServiceResult<RoutePlan> planRoute(GeoCoordinates origin, GeoCoordinates destination,
        TravelMode mode, const std::string& originName, const std::string& destinationName);
    [[nodiscard]] ExternalServiceResult<PreparedRoute> prepareRoute(GeoCoordinates origin, GeoCoordinates destination,
        TravelMode mode, const std::string& originName, const std::string& destinationName);

    Geocoder& m_geocoder;
    RoutePlanner& m_planner;
    RouteHistoryService& m_history;
};

} // namespace msa::core
