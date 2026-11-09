#include "msa/infra/JsonClientPreferences.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr qint64 kMaximumSettingsBytes = 65536;
constexpr int kSettingsVersion = 1;
constexpr int kMaximumPort = 65535;
void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error{"Некорректный файл настроек подключения."};
    }
}
} // namespace

std::optional<core::ClientPreferences> JsonClientPreferences::load(const QString& path)
{
    if (!QFileInfo::exists(path))
    {
        return std::nullopt;
    }
    QFile file{path};
    require(file.open(QIODevice::ReadOnly));
    require(file.size() <= kMaximumSettingsBytes);
    const auto bytes = file.read(kMaximumSettingsBytes + 1);
    require(file.error() == QFileDevice::NoError && bytes.size() <= kMaximumSettingsBytes);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && document.isObject());
    const auto object = document.object();
    require(object.size() == 5 && object.value("version").isDouble() &&
            object.value("version").toDouble() == kSettingsVersion && object.value("host").isString() &&
            object.value("port").isDouble() && object.value("token").isString() &&
            object.value("auto_connect").isBool());
    const auto port = object.value("port").toDouble();
    require(std::isfinite(port) && port >= 1 && port <= kMaximumPort && port == std::floor(port));
    core::ClientPreferences result{object.value("host").toString().toStdString(), static_cast<std::uint16_t>(port),
                                   object.value("token").toString().toStdString(),
                                   object.value("auto_connect").toBool()};
    require(core::validClientPreferences(result));

    return result;
}

void JsonClientPreferences::save(const QString& path, const core::ClientPreferences& preferences)
{
    require(core::validClientPreferences(preferences));
    const auto bytes = QJsonDocument{QJsonObject{{"version", kSettingsVersion},
                                                 {"host", QString::fromStdString(preferences.host)},
                                                 {"port", preferences.port},
                                                 {"token", QString::fromStdString(preferences.token)},
                                                 {"auto_connect", preferences.autoConnect}}}
                           .toJson();
    require(bytes.size() <= kMaximumSettingsBytes);
    QSaveFile file{path};
    require(file.open(QIODevice::WriteOnly));
    require(file.write(bytes) == bytes.size());
    require(file.commit());
}
} // namespace msa::infra
