#include "FakeClock.h"
#include "InMemoryGeocodingCacheRepository.h"

#include "msa/core/CachedGeocoder.h"
#include "msa/infra/HttpGetClient.h"
#include "msa/infra/NominatimGeocoder.h"
#include "msa/infra/OpenMeteoWeatherProvider.h"
#include "msa/infra/OsrmRoutePlanner.h"
#include "msa/infra/SearxngSearchProvider.h"

#include <gtest/gtest.h>

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace msa::infra
{
namespace
{

using namespace std::chrono_literals;

class ServiceReply final : public QNetworkReply
{
public:
    ServiceReply(const QNetworkRequest& request, QByteArray body, int status, NetworkError error, bool stall,
                 bool& aborted, QObject* parent);
    void abort() override;

protected:
    qint64 readData(char* data, qint64 maximum) override;

private:
    QByteArray m_body{};
    qint64 m_offset{};
    bool& m_aborted;
};

ServiceReply::ServiceReply(const QNetworkRequest& request, QByteArray body, int status, NetworkError error, bool stall,
                           bool& aborted, QObject* parent) :
    QNetworkReply{parent}, m_body{std::move(body)}, m_aborted{aborted}
{
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    if (error != NoError)
    {
        setError(error, "SECRET technical response");
    }
    open(QIODevice::ReadOnly);
    if (!stall)
    {
        QTimer::singleShot(0, this,
                           [this]()
                           {
                               setFinished(true);
                               emit finished();
                           });
    }
}

void ServiceReply::abort()
{
    m_aborted = true;
    setError(OperationCanceledError, "SECRET canceled");
    setFinished(true);
    emit finished();
}

qint64 ServiceReply::readData(char* data, qint64 maximum)
{
    const auto count = std::min(maximum, static_cast<qint64>(m_body.size()) - m_offset);
    if (count <= 0)
    {
        return -1;
    }
    std::memcpy(data, m_body.constData() + m_offset, static_cast<std::size_t>(count));
    m_offset += count;

    return count;
}

class ServiceNetwork final : public QNetworkAccessManager
{
public:
    QByteArray response{};
    int status{200};
    QNetworkReply::NetworkError error{QNetworkReply::NoError};
    bool stall{false};
    bool aborted{false};
    std::vector<QNetworkRequest> requests{};
    std::vector<std::chrono::steady_clock::time_point> times{};

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice* outgoing) override;
};

QNetworkReply* ServiceNetwork::createRequest(Operation operation, const QNetworkRequest& request,
                                             [[maybe_unused]] QIODevice* outgoing)
{
    EXPECT_EQ(operation, QNetworkAccessManager::GetOperation);
    requests.push_back(request);
    times.push_back(std::chrono::steady_clock::now());

    return new ServiceReply{request, response, status, error, stall, aborted, this};
}

[[nodiscard]] HttpGetSettings settings()
{
    return {.timeout = 1000ms, .userAgent = "MySuperAssistant/tests"};
}

[[nodiscard]] OsrmEndpoints endpoints()
{
    return {QUrl{"https://route.invalid/foot/route/v1/foot"}, QUrl{"https://route.invalid/bike/route/v1/bike"},
            QUrl{"https://route.invalid/car/route/v1/driving"}};
}

[[nodiscard]] QJsonObject routeObject()
{
    return {{"distance", 1234.5},
            {"duration", 321.5},
            {"legs",
             QJsonArray{QJsonObject{
                 {"steps",
                  QJsonArray{
                      QJsonObject{{"distance", 1234.5},
                                  {"name", "Улица"},
                                  {"maneuver", QJsonObject{{"type", "depart"}, {"modifier", "straight"}}}},
                      QJsonObject{{"distance", 0}, {"name", ""}, {"maneuver", QJsonObject{{"type", "arrive"}}}}}}}}}};
}

[[nodiscard]] QByteArray routeBody(const QJsonObject& route)
{
    return QJsonDocument{QJsonObject{{"code", "Ok"}, {"routes", QJsonArray{route}}}}.toJson();
}

[[nodiscard]] QJsonObject weatherObject()
{
    return {{"current", QJsonObject{{"time", "2026-11-07T12:00"},
                                    {"temperature_2m", -2.5},
                                    {"apparent_temperature", -5},
                                    {"weather_code", 71},
                                    {"wind_speed_10m", 12}}},
            {"daily", QJsonObject{{"time", QJsonArray{"2026-11-07"}},
                                  {"temperature_2m_max", QJsonArray{2}},
                                  {"temperature_2m_min", QJsonArray{-6}},
                                  {"precipitation_probability_max", QJsonArray{30}}}}};
}

template <typename ValueType>
void expectError(const core::ExternalServiceResult<ValueType>& result, core::ExternalServiceErrorKind kind)
{
    ASSERT_TRUE(std::holds_alternative<core::ExternalServiceError>(result));
    const auto& error = std::get<core::ExternalServiceError>(result);
    EXPECT_EQ(error.kind, kind);
    EXPECT_FALSE(error.reason.empty());
    EXPECT_EQ(error.reason.find("SECRET"), std::string::npos);
}

TEST(ExternalServicesTest, GeocodesRussianQueryAndCachesFullCandidatesIncludingZeroCoordinates)
{
    ServiceNetwork network;
    network.response = R"([{"lat":"0","lon":"0","display_name":"Адрес, район, город"},
                           {"lat":"-10.5","lon":"-180","display_name":"Второй"}])";
    NominatimGeocoder source{network, QUrl{"https://geo.invalid/search"}, settings()};
    core::FakeClock clock{std::chrono::local_seconds{}};
    core::InMemoryGeocodingCacheRepository cache;
    core::CachedGeocoder geocoder{source, cache, clock, source.cacheSourceKey()};
    auto result = geocoder.search("  Кафе & парк  ");
    ASSERT_TRUE(std::holds_alternative<std::vector<core::GeocodedPlace>>(result));
    auto& places = std::get<std::vector<core::GeocodedPlace>>(result);
    ASSERT_EQ(places.size(), 2);
    EXPECT_EQ(places[0].name, "Адрес, район, город");
    EXPECT_DOUBLE_EQ(places[0].coordinates.latitude, 0);
    ASSERT_EQ(network.requests.size(), 1);
    const QUrlQuery query{network.requests.back().url()};
    EXPECT_EQ(query.queryItemValue("q", QUrl::FullyDecoded), "Кафе & парк");
    EXPECT_EQ(query.queryItemValue("limit"), "10");
    EXPECT_EQ(query.queryItemValue("accept-language"), "ru");
    EXPECT_EQ(network.requests.back().rawHeader("User-Agent"), settings().userAgent);
    places[0].name = "Изменённая копия";
    result = geocoder.search("Кафе & парк");
    EXPECT_EQ(std::get<std::vector<core::GeocodedPlace>>(result)[0].name, "Адрес, район, город");
    EXPECT_EQ(network.requests.size(), 1);
}

TEST(ExternalServicesTest, GeocoderCachesNoResultsAndLimitsDistinctRequestsToOnePerSecond)
{
    ServiceNetwork network;
    network.response = "[]";
    NominatimGeocoder source{network, QUrl{"https://geo.invalid/search"}, settings()};
    core::FakeClock clock{std::chrono::local_seconds{}};
    core::InMemoryGeocodingCacheRepository cache;
    core::CachedGeocoder geocoder{source, cache, clock, source.cacheSourceKey()};
    expectError(geocoder.search("Первое"), core::ExternalServiceErrorKind::NotFound);
    expectError(geocoder.search("Первое"), core::ExternalServiceErrorKind::NotFound);
    expectError(geocoder.search("Второе"), core::ExternalServiceErrorKind::NotFound);
    ASSERT_EQ(network.times.size(), 2);
    // Небольшой допуск покрывает время между отметкой транспорта и createRequest Qt.
    EXPECT_GE(network.times[1] - network.times[0], 990ms);
}

TEST(ExternalServicesTest, RejectsBadGeocoderJsonFieldsAndCoordinatesWithoutCachingErrors)
{
    ServiceNetwork network;
    for (const auto& body : std::vector<QByteArray>{
             "{", "{}", "[null]", R"([{"lat":0,"lon":"0","display_name":"Адрес"}])",
             R"([{"lat":"NaN","lon":"0","display_name":"Адрес"}])",
             R"([{"lat":"91","lon":"0","display_name":"Адрес"}])",
             R"([{"lat":"0","lon":"bad","display_name":"Адрес"}])", R"([{"lat":"0","lon":"0","display_name":" "}])"})
    {
        network.response = body;
        NominatimGeocoder geocoder{network, QUrl{"https://geo.invalid/search"}, settings()};
        expectError(geocoder.search("Адрес"), core::ExternalServiceErrorKind::InvalidResponse);
    }
    NominatimGeocoder geocoder{network, QUrl{"https://geo.invalid/search"}, settings()};
    network.response = "{";
    expectError(geocoder.search("Повтор"), core::ExternalServiceErrorKind::InvalidResponse);
    network.response = R"([{"lat":"0","lon":"0","display_name":"Адрес"}])";
    EXPECT_TRUE(std::holds_alternative<std::vector<core::GeocodedPlace>>(geocoder.search("Повтор")));
}

TEST(ExternalServicesTest, SendsModeSpecificOsrmCoordinatesInLongitudeLatitudeOrderAndParsesAllSteps)
{
    ServiceNetwork network;
    network.response = routeBody(routeObject());
    OsrmRoutePlanner planner{network, endpoints(), settings()};
    const std::vector<std::pair<core::TravelMode, QString>> modes{
        {core::TravelMode::Foot, "/foot/route/v1/foot/"},
        {core::TravelMode::Bike, "/bike/route/v1/bike/"},
        {core::TravelMode::Driving, "/car/route/v1/driving/"}};
    for (const auto& [mode, prefix] : modes)
    {
        const auto result = planner.route({55, 82}, {56, 83}, mode);
        ASSERT_TRUE(std::holds_alternative<core::RoutePlan>(result));
        const auto& route = std::get<core::RoutePlan>(result);
        EXPECT_DOUBLE_EQ(route.distanceMeters, 1234.5);
        EXPECT_DOUBLE_EQ(route.durationSeconds, 321.5);
        EXPECT_FALSE(route.approximate);
        ASSERT_EQ(route.steps.size(), 2);
        EXPECT_EQ(route.steps[0].street, "Улица");
        EXPECT_EQ(route.steps[1].type, "arrive");
        const auto url = network.requests.back().url();
        EXPECT_EQ(url.path(), prefix + "82.000000,55.000000;83.000000,56.000000");
        EXPECT_EQ(QUrlQuery{url}.queryItemValue("steps"), "true");
        EXPECT_EQ(QUrlQuery{url}.queryItemValue("overview"), "false");
    }
}

TEST(ExternalServicesTest, HandlesNoRouteAndRejectsBrokenOrIncompleteRoutes)
{
    ServiceNetwork network;
    OsrmRoutePlanner planner{network, endpoints(), settings()};
    network.response = R"({"code":"NoRoute"})";
    expectError(planner.route({0, 0}, {0, 1}, core::TravelMode::Foot), core::ExternalServiceErrorKind::NotFound);
    for (const auto& body : std::vector<QByteArray>{"{", "[]", "{}", R"({"code":"Ok","routes":[]})"})
    {
        network.response = body;
        expectError(planner.route({0, 0}, {0, 1}, core::TravelMode::Foot),
                    core::ExternalServiceErrorKind::InvalidResponse);
    }
    for (const auto& key : {"distance", "duration", "legs"})
    {
        auto route = routeObject();
        route.remove(key);
        network.response = routeBody(route);
        expectError(planner.route({0, 0}, {0, 1}, core::TravelMode::Foot),
                    core::ExternalServiceErrorKind::InvalidResponse);
    }
    auto route = routeObject();
    route["distance"] = -1;
    network.response = routeBody(route);
    expectError(planner.route({0, 0}, {0, 1}, core::TravelMode::Foot), core::ExternalServiceErrorKind::InvalidResponse);
}

TEST(ExternalServicesTest, RejectsInvalidManeuversAndPreservesUnknownManeuverTypes)
{
    ServiceNetwork network;
    OsrmRoutePlanner planner{network, endpoints(), settings()};
    for (const auto& maneuver : std::vector<QJsonObject>{{},
                                                         {{"type", 1}},
                                                         {{"type", "turn"}, {"modifier", 1}},
                                                         {{"type", "roundabout"}, {"exit", 1.5}},
                                                         {{"type", "roundabout"}, {"exit", 0}}})
    {
        auto route = routeObject();
        route["legs"] = QJsonArray{QJsonObject{
            {"steps", QJsonArray{QJsonObject{{"distance", 10}, {"name", "Улица"}, {"maneuver", maneuver}}}}}};
        network.response = routeBody(route);
        expectError(planner.route({0, 0}, {0, 1}, core::TravelMode::Foot),
                    core::ExternalServiceErrorKind::InvalidResponse);
    }
    auto route = routeObject();
    route["legs"] = QJsonArray{QJsonObject{
        {"steps", QJsonArray{QJsonObject{
                      {"distance", 10}, {"name", "Улица"}, {"maneuver", QJsonObject{{"type", "future maneuver"}}}}}}}};
    network.response = routeBody(route);
    const auto result = planner.route({0, 0}, {0, 1}, core::TravelMode::Foot);
    ASSERT_TRUE(std::holds_alternative<core::RoutePlan>(result));
    EXPECT_EQ(std::get<core::RoutePlan>(result).steps.front().type, "future maneuver");
}

TEST(ExternalServicesTest, RequestsExplicitWeatherUnitsAndOneLocalDayAndParsesForecast)
{
    ServiceNetwork network;
    network.response = QJsonDocument{weatherObject()}.toJson();
    OpenMeteoWeatherProvider provider{network, QUrl{"https://weather.invalid/v1/forecast"}, settings()};
    const auto result = provider.forecast({0, 0});
    ASSERT_TRUE(std::holds_alternative<core::WeatherForecast>(result));
    const auto& forecast = std::get<core::WeatherForecast>(result);
    EXPECT_DOUBLE_EQ(forecast.temperatureCelsius, -2.5);
    EXPECT_DOUBLE_EQ(forecast.apparentTemperatureCelsius, -5);
    EXPECT_EQ(forecast.weatherCode, 71);
    EXPECT_EQ(forecast.maximumTemperatureCelsius, 2);
    EXPECT_EQ(forecast.minimumTemperatureCelsius, -6);
    EXPECT_EQ(forecast.precipitationProbabilityPercent, 30);
    const QUrlQuery query{network.requests.back().url()};
    EXPECT_EQ(query.queryItemValue("latitude"), "0.000000");
    EXPECT_EQ(query.queryItemValue("longitude"), "0.000000");
    EXPECT_EQ(query.queryItemValue("timezone"), "auto");
    EXPECT_EQ(query.queryItemValue("forecast_days"), "1");
    EXPECT_EQ(query.queryItemValue("temperature_unit"), "celsius");
    EXPECT_EQ(query.queryItemValue("wind_speed_unit"), "kmh");
}

TEST(ExternalServicesTest, AllowsAbsentDailyAndNullDailyValuesButRejectsMalformedDailyData)
{
    ServiceNetwork network;
    OpenMeteoWeatherProvider provider{network, QUrl{"https://weather.invalid/v1/forecast"}, settings()};
    auto root = weatherObject();
    root.remove("daily");
    network.response = QJsonDocument{root}.toJson();
    auto result = provider.forecast({0, 0});
    ASSERT_TRUE(std::holds_alternative<core::WeatherForecast>(result));
    EXPECT_FALSE(std::get<core::WeatherForecast>(result).maximumTemperatureCelsius);
    root = weatherObject();
    auto daily = root["daily"].toObject();
    daily["temperature_2m_max"] = QJsonArray{QJsonValue::Null};
    daily["temperature_2m_min"] = QJsonArray{QJsonValue::Null};
    daily["precipitation_probability_max"] = QJsonArray{QJsonValue::Null};
    root["daily"] = daily;
    network.response = QJsonDocument{root}.toJson();
    EXPECT_TRUE(std::holds_alternative<core::WeatherForecast>(provider.forecast({0, 0})));
    for (const auto& replacement : std::vector<QJsonObject>{{{"temperature_2m_max", QJsonArray{}}},
                                                            {{"temperature_2m_min", QJsonArray{10}}},
                                                            {{"temperature_2m_min", QJsonArray{QJsonValue::Null}}},
                                                            {{"precipitation_probability_max", QJsonArray{101}}},
                                                            {{"precipitation_probability_max", QJsonArray{1.5}}},
                                                            {{"time", QJsonArray{"2026-11-06"}}}})
    {
        root = weatherObject();
        daily = root["daily"].toObject();
        for (auto iterator = replacement.begin(); iterator != replacement.end(); ++iterator)
        {
            daily[iterator.key()] = iterator.value();
        }
        root["daily"] = daily;
        network.response = QJsonDocument{root}.toJson();
        expectError(provider.forecast({0, 0}), core::ExternalServiceErrorKind::InvalidResponse);
    }
}

TEST(ExternalServicesTest, RejectsMissingNullStringOrFractionalCurrentWeatherValues)
{
    ServiceNetwork network;
    OpenMeteoWeatherProvider provider{network, QUrl{"https://weather.invalid/v1/forecast"}, settings()};
    for (const auto& key : {"temperature_2m", "apparent_temperature", "weather_code", "wind_speed_10m"})
    {
        for (const auto& value : std::vector<QJsonValue>{QJsonValue::Undefined, QJsonValue::Null, "0"})
        {
            auto root = weatherObject();
            auto current = root["current"].toObject();
            current[key] = value;
            root["current"] = current;
            network.response = QJsonDocument{root}.toJson();
            expectError(provider.forecast({0, 0}), core::ExternalServiceErrorKind::InvalidResponse);
        }
    }
    auto root = weatherObject();
    auto current = root["current"].toObject();
    current["weather_code"] = 1.5;
    root["current"] = current;
    network.response = QJsonDocument{root}.toJson();
    expectError(provider.forecast({0, 0}), core::ExternalServiceErrorKind::InvalidResponse);
}

TEST(ExternalServicesTest, InvalidInputsNeverReachNetwork)
{
    ServiceNetwork network;
    NominatimGeocoder geocoder{network, QUrl{"https://geo.invalid/search"}, settings()};
    OsrmRoutePlanner planner{network, endpoints(), settings()};
    OpenMeteoWeatherProvider weather{network, QUrl{"https://weather.invalid/forecast"}, settings()};
    expectError(geocoder.search(" \n "), core::ExternalServiceErrorKind::InvalidRequest);
    expectError(planner.route({91, 0}, {0, 0}, core::TravelMode::Foot), core::ExternalServiceErrorKind::InvalidRequest);
    expectError(weather.forecast({0, 181}), core::ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(network.requests.empty());
}

TEST(ExternalServicesTest, HttpErrorsHaveNoSecretsRetriesOrRedirects)
{
    ServiceNetwork network;
    network.response = "SECRET body";
    HttpGetClient client{network, settings()};
    for (const auto status : {301, 401, 429, 500, 503})
    {
        network.status = status;
        expectError(client.get(QUrl{"https://service.invalid/path"}), core::ExternalServiceErrorKind::Unavailable);
    }
    network.status = 200;
    network.error = QNetworkReply::ConnectionRefusedError;
    expectError(client.get(QUrl{"https://service.invalid/path"}), core::ExternalServiceErrorKind::Unavailable);
    EXPECT_EQ(network.requests.size(), 6);
    EXPECT_EQ(network.requests.back().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
              QNetworkRequest::ManualRedirectPolicy);
}

TEST(ExternalServicesTest, HttpDeadlineAbortsAndReentryDoesNotSendAnotherRequest)
{
    ServiceNetwork network;
    network.stall = true;
    auto options = settings();
    options.timeout = 10ms;
    HttpGetClient client{network, options};
    std::optional<core::ExternalServiceResult<QByteArray>> nested;
    QTimer::singleShot(0, &network,
                       [&client, &nested]() { nested = client.get(QUrl{"https://service.invalid/path"}); });
    expectError(client.get(QUrl{"https://service.invalid/path"}), core::ExternalServiceErrorKind::Unavailable);
    ASSERT_TRUE(nested);
    expectError(*nested, core::ExternalServiceErrorKind::Unavailable);
    EXPECT_TRUE(network.aborted);
    EXPECT_EQ(network.requests.size(), 1);
    network.stall = false;
    network.response = "{}";
    EXPECT_TRUE(std::holds_alternative<QByteArray>(client.get(QUrl{"https://service.invalid/path"})));
}

TEST(ExternalServicesTest, RejectsInvalidSettingsAndEndpoints)
{
    ServiceNetwork network;
    for (const auto& address : {"", "file:///tmp/service", "http://", "https://user:SECRET@host/path",
                                "https://host/path?SECRET", "https://host/path#fragment"})
    {
        EXPECT_THROW((NominatimGeocoder{network, QUrl{address}, settings()}), std::invalid_argument);
        EXPECT_THROW((OpenMeteoWeatherProvider{network, QUrl{address}, settings()}), std::invalid_argument);
    }
    auto invalidEndpoints = endpoints();
    invalidEndpoints.bike = QUrl{};
    EXPECT_THROW((OsrmRoutePlanner{network, invalidEndpoints, settings()}), std::invalid_argument);
    auto invalidSettings = settings();
    invalidSettings.timeout = 0ms;
    EXPECT_THROW((HttpGetClient{network, invalidSettings}), std::invalid_argument);
    invalidSettings = settings();
    invalidSettings.userAgent = "test\r\nSECRET";
    EXPECT_THROW((HttpGetClient{network, invalidSettings}), std::invalid_argument);
    EXPECT_TRUE(network.requests.empty());
}

TEST(ExternalServicesTest, SearxngSendsOnlyExplicitPhraseAndParsesThreeUsableSources)
{
    ServiceNetwork network;
    network.response = QJsonDocument{QJsonObject{{"results", QJsonArray{
        QJsonObject{{"title", "Карточка без ссылки"}, {"url", QJsonValue::Null}},
        QJsonObject{{"title", ""}, {"url", "https://example.org/skip"}},
        QJsonObject{{"title", "Неверный адрес"}, {"url", "javascript:alert(1)"}},
        QJsonObject{{"title", "Первый"}, {"url", "https://example.org/1"}, {"content", "Описание"}},
        QJsonObject{{"title", "Второй"}, {"url", "https://example.org/2"}},
        QJsonObject{{"title", "Третий"}, {"url", "https://example.org/3"}},
        QJsonObject{{"title", "Четвёртый"}, {"url", "https://example.org/4"}}}}}}.toJson();
    SearxngSearchProvider provider{network, QUrl{"https://search.invalid/search"}, settings()};
    const auto result = provider.search("  новости C++ & Qt %2B  ");
    ASSERT_TRUE(std::holds_alternative<std::vector<core::WebSearchResult>>(result));
    const auto& sources = std::get<std::vector<core::WebSearchResult>>(result);
    ASSERT_EQ(sources.size(), 3);
    EXPECT_EQ(sources[0].title, "Первый");
    EXPECT_EQ(sources[0].snippet, "Описание");
    EXPECT_TRUE(sources[1].snippet.empty());
    EXPECT_EQ(sources[2].url, "https://example.org/3");
    ASSERT_EQ(network.requests.size(), 1);
    const QUrlQuery query{network.requests.front().url()};
    EXPECT_EQ(query.queryItemValue("q", QUrl::FullyDecoded), "новости C++ & Qt %2B");
    EXPECT_TRUE(network.requests.front().url().toEncoded().contains("C%2B%2B"));
    EXPECT_TRUE(network.requests.front().url().toEncoded().contains("%252B"));
    auto formEncoded = query.queryItemValue("q", QUrl::FullyEncoded).toUtf8();
    formEncoded.replace('+', ' ');
    EXPECT_EQ(QUrl::fromPercentEncoding(formEncoded), "новости C++ & Qt %2B");
    EXPECT_EQ(query.queryItemValue("format"), "json");
    EXPECT_EQ(query.queryItems().size(), 2);
    EXPECT_EQ(network.requests.front().url().path(), "/search");
}

TEST(ExternalServicesTest, SearxngRejectsMalformedResultsAndReportsNoUsableSources)
{
    ServiceNetwork network;
    SearxngSearchProvider provider{network, QUrl{"https://search.invalid/search"}, settings()};
    for (const auto& body : std::vector<QByteArray>{"{", "[]", "{}", R"({"results":null})",
             R"({"results":[null]})", R"({"results":[{"title":42}]})",
             R"({"results":[{"title":"Title","url":"https://example.org/","content":42}]})"}) {
        network.response = body;
        expectError(provider.search("Фраза"), core::ExternalServiceErrorKind::InvalidResponse);
    }
    for (const auto& body : std::vector<QByteArray>{R"({"results":[]})",
             R"({"results":[{"title":"Title","url":"https://user:SECRET@example.org/"}]})"}) {
        network.response = body;
        expectError(provider.search("Фраза"), core::ExternalServiceErrorKind::NotFound);
    }
}

TEST(ExternalServicesTest, SearxngDoesNotRetryHttpFailuresAndRejectsInvalidInputBeforeNetwork)
{
    ServiceNetwork network;
    SearxngSearchProvider provider{network, QUrl{"https://search.invalid/search"}, settings()};
    expectError(provider.search(" \n "), core::ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(network.requests.empty());
    network.status = 403;
    network.response = "SECRET technical error";
    expectError(provider.search("Фраза"), core::ExternalServiceErrorKind::Unavailable);
    EXPECT_EQ(network.requests.size(), 1);
    EXPECT_THROW((SearxngSearchProvider{network, QUrl{"file:///search"}, settings()}), std::invalid_argument);
}

} // namespace
} // namespace msa::infra
