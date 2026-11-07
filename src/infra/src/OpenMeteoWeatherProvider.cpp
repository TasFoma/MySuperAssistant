#include "msa/infra/OpenMeteoWeatherProvider.h"

#include <QDate>
#include <QDateTime>
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
    return {core::ExternalServiceErrorKind::InvalidResponse, "Сервис погоды вернул неверный ответ."};
}

[[nodiscard]] bool finiteNumber(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

[[nodiscard]] bool optionalDailyValue(const QJsonObject& daily, const QString& key, QJsonValue& output)
{
    if (!daily.contains(key))
    {
        return true;
    }
    const auto value = daily[key];
    if (!value.isArray() || value.toArray().size() != 1)
    {
        return false;
    }
    output = value.toArray().first();

    return output.isNull() || finiteNumber(output);
}

} // namespace

OpenMeteoWeatherProvider::OpenMeteoWeatherProvider(QNetworkAccessManager& manager, QUrl endpoint,
                                                   HttpGetSettings settings) :
    m_http{manager, std::move(settings)}, m_endpoint{std::move(endpoint)}
{
    if (!HttpGetClient::validEndpoint(m_endpoint))
    {
        throw std::invalid_argument{"Некорректный адрес Open-Meteo."};
    }
}

core::ExternalServiceResult<core::WeatherForecast>
OpenMeteoWeatherProvider::requestForecast(core::GeoCoordinates coordinates)
{
    constexpr int kCoordinatePrecision = 6;
    auto url = m_endpoint;
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(coordinates.latitude, 'f', kCoordinatePrecision));
    query.addQueryItem("longitude", QString::number(coordinates.longitude, 'f', kCoordinatePrecision));
    query.addQueryItem("current", "temperature_2m,apparent_temperature,weather_code,wind_speed_10m");
    query.addQueryItem("daily", "temperature_2m_max,temperature_2m_min,precipitation_probability_max");
    query.addQueryItem("timezone", "auto");
    query.addQueryItem("forecast_days", "1");
    query.addQueryItem("temperature_unit", "celsius");
    query.addQueryItem("wind_speed_unit", "kmh");
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
    if (root.contains("error") || !root["current"].isObject())
    {
        return invalidResponse();
    }
    const auto current = root["current"].toObject();
    if (!finiteNumber(current["temperature_2m"]) || !finiteNumber(current["apparent_temperature"]) ||
        !finiteNumber(current["wind_speed_10m"]) || !current["weather_code"].isDouble())
    {
        return invalidResponse();
    }
    const auto code = current["weather_code"].toInteger(-1);
    if (code < 0 || code > std::numeric_limits<int>::max())
    {
        return invalidResponse();
    }
    core::WeatherForecast forecast{.temperatureCelsius = current["temperature_2m"].toDouble(),
                                   .apparentTemperatureCelsius = current["apparent_temperature"].toDouble(),
                                   .weatherCode = static_cast<int>(code),
                                   .windSpeedKilometersPerHour = current["wind_speed_10m"].toDouble()};
    if (!root.contains("daily"))
    {
        return forecast;
    }
    if (!root["daily"].isObject())
    {
        return invalidResponse();
    }
    const auto daily = root["daily"].toObject();
    if (!current["time"].isString() || !QDateTime::fromString(current["time"].toString(), Qt::ISODate).isValid() ||
        !daily["time"].isArray() || daily["time"].toArray().size() != 1 || !daily["time"].toArray().first().isString())
    {
        return invalidResponse();
    }
    const auto date = QDate::fromString(daily["time"].toArray().first().toString(), Qt::ISODate);
    if (!date.isValid() || date != QDateTime::fromString(current["time"].toString(), Qt::ISODate).date())
    {
        return invalidResponse();
    }
    QJsonValue maximum;
    QJsonValue minimum;
    QJsonValue precipitation;
    if (!optionalDailyValue(daily, "temperature_2m_max", maximum) ||
        !optionalDailyValue(daily, "temperature_2m_min", minimum) ||
        !optionalDailyValue(daily, "precipitation_probability_max", precipitation))
    {
        return invalidResponse();
    }
    const auto hasMaximum = !maximum.isNull() && !maximum.isUndefined();
    const auto hasMinimum = !minimum.isNull() && !minimum.isUndefined();
    if (hasMaximum != hasMinimum)
    {
        return invalidResponse();
    }
    if (hasMaximum)
    {
        forecast.maximumTemperatureCelsius = maximum.toDouble();
        forecast.minimumTemperatureCelsius = minimum.toDouble();
    }
    if (!precipitation.isNull() && !precipitation.isUndefined())
    {
        const auto probability = precipitation.toInteger(-1);
        if (probability < 0 || probability > 100)
        {
            return invalidResponse();
        }
        forecast.precipitationProbabilityPercent = static_cast<int>(probability);
    }

    return forecast;
}

} // namespace msa::infra
