#include "msa/infra/NominatimGeocoder.h"

#include "msa/core/NavigationService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

constexpr auto kRequestInterval = std::chrono::milliseconds{1000};

[[nodiscard]] core::ExternalServiceError invalidResponse()
{
    return {core::ExternalServiceErrorKind::InvalidResponse, "Геокодер вернул неверный ответ."};
}

} // namespace

NominatimGeocoder::NominatimGeocoder(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings) :
    m_http{manager, std::move(settings)}, m_endpoint{std::move(endpoint)}
{
    if (!HttpGetClient::validEndpoint(m_endpoint))
    {
        throw std::invalid_argument{"Некорректный адрес Nominatim."};
    }
}

core::ExternalServiceResult<std::vector<core::GeocodedPlace>> NominatimGeocoder::requestPlaces(const std::string& place)
{
    if (const auto cached = m_cache.find(place); cached != m_cache.end())
    {
        return cached->second;
    }
    auto url = m_endpoint;
    QUrlQuery query;
    query.addQueryItem("q", QString::fromStdString(place));
    query.addQueryItem("format", "json");
    query.addQueryItem("limit", "10");
    query.addQueryItem("accept-language", "ru");
    url.setQuery(query);
    const auto result = m_http.get(url, kRequestInterval);
    if (const auto* error = std::get_if<core::ExternalServiceError>(&result))
    {
        return *error;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(std::get<QByteArray>(result), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
    {
        return invalidResponse();
    }
    std::vector<core::GeocodedPlace> places;
    for (const auto& value : document.array())
    {
        if (!value.isObject())
        {
            return invalidResponse();
        }
        const auto object = value.toObject();
        if (!object["lat"].isString() || !object["lon"].isString() || !object["display_name"].isString() ||
            object["display_name"].toString().trimmed().isEmpty())
        {
            return invalidResponse();
        }
        bool latitudeOk = false;
        bool longitudeOk = false;
        const auto latitude = object["lat"].toString().toDouble(&latitudeOk);
        const auto longitude = object["lon"].toString().toDouble(&longitudeOk);
        if (!latitudeOk || !longitudeOk || !core::validCoordinates({latitude, longitude}))
        {
            return invalidResponse();
        }
        places.push_back({{latitude, longitude}, object["display_name"].toString().toStdString()});
    }
    m_cache.emplace(place, places);

    return places;
}

} // namespace msa::infra
