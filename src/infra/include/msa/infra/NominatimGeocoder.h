#pragma once

#include "msa/core/Geocoder.h"
#include "msa/infra/HttpGetClient.h"

#include <map>
#include <string>
#include <vector>

namespace msa::infra
{

/// @brief Nominatim: до десяти русскоязычных кандидатов, кэш и интервал 1 секунда.
/// Один экземпляр на приложение обслуживает все пользовательские запросы.
/// Кэш успешных ответов живёт до уничтожения экземпляра, включая отсутствие мест.
class NominatimGeocoder final : public core::Geocoder
{
public:
    /// @brief Endpoint — полный адрес /search; неверный адрес вызывает invalid_argument.
    NominatimGeocoder(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings);

protected:
    /// @brief Запрашивает кандидатов, сохраняя полные адреса и точные координаты.
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::GeocodedPlace>>
    requestPlaces(const std::string& place) override;

private:
    HttpGetClient m_http;
    QUrl m_endpoint{};
    std::map<std::string, std::vector<core::GeocodedPlace>> m_cache{};
};

} // namespace msa::infra
