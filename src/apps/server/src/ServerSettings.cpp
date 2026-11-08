#include "ServerSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimeZone>

#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace msa::server {
namespace {
constexpr qint64 kMaximumSettingsBytes{1048576};
constexpr int kMaximumPort{65535};
constexpr int kMaximumTimeoutMs{3600000};
constexpr int kMaximumBudget{100000000};

[[noreturn]] void invalidSettings()
{
    throw std::invalid_argument{"Неверный файл настроек сервера. Проверьте поля по примеру."};
}

void fields(const QJsonObject& object, std::initializer_list<const char*> names)
{
    if (object.size() != static_cast<qsizetype>(names.size())) { invalidSettings(); }
    for (const auto* name : names) {
        if (!object.contains(QLatin1String{name})) { invalidSettings(); }
    }
}

QString string(const QJsonObject& object, const char* name, bool allowEmpty = false)
{
    const auto value = object.value(QLatin1String{name});
    if (!value.isString()) { invalidSettings(); }
    const auto result = value.toString();
    if ((!allowEmpty && result.trimmed().isEmpty()) || result.contains('<') || result.contains('\n')
        || result.contains('\r') || result.contains(QChar::Null)) { invalidSettings(); }

    return result;
}

int integer(const QJsonObject& object, const char* name, int minimum, int maximum)
{
    const auto value = object.value(QLatin1String{name});
    const auto number = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
        || number < minimum || number > maximum) { invalidSettings(); }

    return static_cast<int>(number);
}

QUrl endpoint(const QJsonObject& object, const char* name)
{
    const QUrl result{string(object, name), QUrl::StrictMode};
    if (!infra::HttpGetClient::validEndpoint(result)) { invalidSettings(); }

    return result;
}
} // namespace

ServerSettings readServerSettings(const QString& filePath)
{
    QFile file{filePath};
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaximumSettingsBytes) { invalidSettings(); }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { invalidSettings(); }
    const auto root = document.object();
    fields(root, {"listen_address", "port", "token", "time_zone", "database_path", "context_max_chars",
        "check_interval_ms", "llama", "services"});
    ServerSettings result;
    if (!result.listenAddress.setAddress(string(root, "listen_address"))) { invalidSettings(); }
    result.port = static_cast<quint16>(integer(root, "port", 0, kMaximumPort));
    result.token = string(root, "token");
    result.timeZone = string(root, "time_zone");
    if (!QTimeZone::isTimeZoneIdAvailable(result.timeZone.toUtf8())) { invalidSettings(); }
    const auto path = string(root, "database_path");
    result.databasePath = QFileInfo{filePath}.absoluteDir().absoluteFilePath(path);
    result.contextMaxChars = static_cast<std::size_t>(integer(root, "context_max_chars", 1, kMaximumBudget));
    result.checkIntervalMs = integer(root, "check_interval_ms", 1, kMaximumTimeoutMs);
    const auto llama = root.value("llama").toObject();
    fields(llama, {"completion_url", "model", "api_key", "timeout_ms", "max_response_tokens"});
    result.llama.completionUrl = endpoint(llama, "completion_url");
    result.llama.model = string(llama, "model", true);
    result.llama.apiKey = string(llama, "api_key", true).toUtf8();
    result.llama.timeout = std::chrono::milliseconds{integer(llama, "timeout_ms", 1, kMaximumTimeoutMs)};
    result.llama.maxResponseTokens = integer(llama, "max_response_tokens", 1, kMaximumBudget);
    const auto services = root.value("services").toObject();
    fields(services, {"timeout_ms", "user_agent", "nominatim", "osrm_foot", "osrm_bike", "osrm_driving",
        "weather", "search"});
    result.http.timeout = std::chrono::milliseconds{integer(services, "timeout_ms", 1, kMaximumTimeoutMs)};
    result.http.userAgent = string(services, "user_agent").toUtf8();
    result.nominatim = endpoint(services, "nominatim");
    result.osrm = {endpoint(services, "osrm_foot"), endpoint(services, "osrm_bike"), endpoint(services, "osrm_driving")};
    result.weather = endpoint(services, "weather");
    result.search = endpoint(services, "search");

    return result;
}
} // namespace msa::server

