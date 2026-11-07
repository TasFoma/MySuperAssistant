#pragma once

#include "msa/core/WeatherProvider.h"
#include "msa/infra/HttpGetClient.h"

namespace msa::infra
{

/// @brief Open-Meteo: текущая погода и дневные показатели в местной зоне точки.
class OpenMeteoWeatherProvider final : public core::WeatherProvider
{
public:
    /// @brief Endpoint — полный адрес /v1/forecast; неверный вызывает invalid_argument.
    OpenMeteoWeatherProvider(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings);

protected:
    /// @brief Один GET с явными единицами °C и км/ч, timezone=auto и forecast_days=1.
    [[nodiscard]] core::ExternalServiceResult<core::WeatherForecast>
    requestForecast(core::GeoCoordinates coordinates) override;

private:
    HttpGetClient m_http;
    QUrl m_endpoint{};
};

} // namespace msa::infra
