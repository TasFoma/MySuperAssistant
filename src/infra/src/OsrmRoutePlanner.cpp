#include "msa/infra/OsrmRoutePlanner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

[[nodiscard]] core::ExternalServiceError invalidResponse()
{
    return {core::ExternalServiceErrorKind::InvalidResponse, "Сервис маршрутов вернул неверный ответ."};
}

[[nodiscard]] bool nonnegativeNumber(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= 0;
}

[[nodiscard]] QString coordinateText(core::GeoCoordinates coordinates)
{
    constexpr int kCoordinatePrecision = 6;

    return QString::number(coordinates.longitude, 'f', kCoordinatePrecision) + "," +
           QString::number(coordinates.latitude, 'f', kCoordinatePrecision);
}

} // namespace

OsrmRoutePlanner::OsrmRoutePlanner(QNetworkAccessManager& manager, OsrmEndpoints endpoints, HttpGetSettings settings) :
    m_http{manager, std::move(settings)}, m_endpoints{std::move(endpoints)}
{
    if (!HttpGetClient::validEndpoint(m_endpoints.foot) || !HttpGetClient::validEndpoint(m_endpoints.bike) ||
        !HttpGetClient::validEndpoint(m_endpoints.driving))
    {
        throw std::invalid_argument{"Некорректные адреса профилей OSRM."};
    }
}

core::ExternalServiceResult<core::RoutePlan>
OsrmRoutePlanner::requestRoute(core::GeoCoordinates origin, core::GeoCoordinates destination, core::TravelMode mode)
{
    auto url = mode == core::TravelMode::Driving ? m_endpoints.driving
               : mode == core::TravelMode::Bike  ? m_endpoints.bike
                                                 : m_endpoints.foot;
    auto path = url.path();
    if (!path.endsWith('/'))
    {
        path += '/';
    }
    url.setPath(path + coordinateText(origin) + ";" + coordinateText(destination));
    QUrlQuery query;
    query.addQueryItem("steps", "true");
    query.addQueryItem("overview", "false");
    url.setQuery(query);
    const auto result = m_http.get(url);
    if (const auto* error = std::get_if<core::ExternalServiceError>(&result))
    {
        return *error;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(std::get<QByteArray>(result), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return invalidResponse();
    }
    const auto root = document.object();
    if (root["code"] == "NoRoute" || root["code"] == "NoSegment")
    {
        return core::ExternalServiceError{core::ExternalServiceErrorKind::NotFound,
                                          "Не получилось построить маршрут между этими точками."};
    }
    if (root["code"] != "Ok" || !root["routes"].isArray() || root["routes"].toArray().isEmpty() ||
        !root["routes"].toArray().first().isObject())
    {
        return invalidResponse();
    }
    const auto route = root["routes"].toArray().first().toObject();
    if (!nonnegativeNumber(route["distance"]) || !nonnegativeNumber(route["duration"]) || !route["legs"].isArray() ||
        route["legs"].toArray().isEmpty())
    {
        return invalidResponse();
    }
    core::RoutePlan plan{.distanceMeters = route["distance"].toDouble(),
                         .durationSeconds = route["duration"].toDouble()};
    for (const auto& legValue : route["legs"].toArray())
    {
        if (!legValue.isObject() || !legValue.toObject()["steps"].isArray() ||
            legValue.toObject()["steps"].toArray().isEmpty())
        {
            return invalidResponse();
        }
        for (const auto& stepValue : legValue.toObject()["steps"].toArray())
        {
            if (!stepValue.isObject())
            {
                return invalidResponse();
            }
            const auto step = stepValue.toObject();
            if (!step["maneuver"].isObject() || !step["name"].isString() || !nonnegativeNumber(step["distance"]))
            {
                return invalidResponse();
            }
            const auto maneuver = step["maneuver"].toObject();
            if (!maneuver["type"].isString() || maneuver["type"].toString().trimmed().isEmpty() ||
                (maneuver.contains("modifier") && !maneuver["modifier"].isString()))
            {
                return invalidResponse();
            }
            core::RouteStep parsed{.type = maneuver["type"].toString().toStdString(),
                                   .modifier = maneuver["modifier"].toString().toStdString(),
                                   .street = step["name"].toString().toStdString(),
                                   .distanceMeters = step["distance"].toDouble()};
            if (maneuver.contains("exit"))
            {
                const auto exitNumber = maneuver["exit"].toInteger(0);
                if (!maneuver["exit"].isDouble() || exitNumber <= 0 || exitNumber > std::numeric_limits<int>::max())
                {
                    return invalidResponse();
                }
                parsed.exitNumber = static_cast<int>(exitNumber);
            }
            plan.steps.push_back(std::move(parsed));
        }
    }

    return plan;
}

} // namespace msa::infra
