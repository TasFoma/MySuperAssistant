#pragma once

#include "msa/core/RoutePlanner.h"
#include "msa/infra/HttpGetClient.h"

namespace msa::infra
{

/// @brief Полные префиксы /route/v1/<профиль> нужных серверов без координат.
struct OsrmEndpoints
{
    /// @brief Пеший профиль, например адрес отдельного routed-foot.
    QUrl foot{};
    /// @brief Велосипедный профиль.
    QUrl bike{};
    /// @brief Автомобильный профиль.
    QUrl driving{};
};

/// @brief OSRM: первый маршрут, все манёвры, метры и секунды без персонализации.
class OsrmRoutePlanner final : public core::RoutePlanner
{
public:
    /// @brief Все три endpoint обязательны; неверные вызывают invalid_argument.
    OsrmRoutePlanner(QNetworkAccessManager& manager, OsrmEndpoints endpoints, HttpGetSettings settings);

protected:
    /// @brief Один GET с steps=true и overview=false.
    [[nodiscard]] core::ExternalServiceResult<core::RoutePlan>
    requestRoute(core::GeoCoordinates origin, core::GeoCoordinates destination, core::TravelMode mode) override;

private:
    HttpGetClient m_http;
    OsrmEndpoints m_endpoints{};
};

} // namespace msa::infra
