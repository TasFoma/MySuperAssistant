#pragma once

#include "msa/core/Geocoder.h"
#include "msa/infra/HttpGetClient.h"

#include <string>
#include <vector>

namespace msa::infra
{

/// @brief Nominatim: до десяти русскоязычных кандидатов и интервал 1 секунда.
/// Один экземпляр на приложение обслуживает все пользовательские запросы.
/// Пользовательские запросы должны проходить через core::CachedGeocoder.
class NominatimGeocoder final : public core::Geocoder
{
public:
    /// @brief Endpoint — полный адрес /search; неверный адрес вызывает invalid_argument.
    NominatimGeocoder(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings);
    /// @brief Ключ endpoint и фиксированных параметров для постоянного кэша.
    [[nodiscard]] std::string cacheSourceKey() const;

protected:
    /// @brief Запрашивает кандидатов, сохраняя полные адреса и точные координаты.
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::GeocodedPlace>>
    requestPlaces(const std::string& place) override;

private:
    HttpGetClient m_http;
    QUrl m_endpoint{};
};

} // namespace msa::infra
