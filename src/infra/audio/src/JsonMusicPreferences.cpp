#include "msa/infra/JsonMusicPreferences.h"

#include <QDir>
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
constexpr qint64 kMaximumBytes = 65536;
constexpr int kVersion = 1;
void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error{"Не удалось прочитать или сохранить настройки музыки."};
    }
}
void validate(const core::MusicPreferences& preferences)
{
    const auto folder = QString::fromStdString(preferences.folder);
    require(!folder.contains(QChar::Null) && (folder.isEmpty() || QDir::isAbsolutePath(folder)) &&
            std::isfinite(preferences.volume) && preferences.volume >= 0 && preferences.volume <= 1);
}
} // namespace
std::optional<core::MusicPreferences> JsonMusicPreferences::load(const QString& path)
{
    if (!QFileInfo::exists(path))
    {
        return std::nullopt;
    }
    QFile file{path};
    require(file.open(QIODevice::ReadOnly) && file.size() <= kMaximumBytes);
    const auto bytes = file.read(kMaximumBytes + 1);
    require(file.error() == QFileDevice::NoError && bytes.size() <= kMaximumBytes);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && document.isObject());
    const auto object = document.object();
    require(object.size() == 3 && object.value("version").isDouble() &&
            object.value("version").toDouble() == kVersion && object.value("folder").isString() &&
            object.value("volume").isDouble());
    core::MusicPreferences result{object.value("folder").toString().toStdString(), object.value("volume").toDouble()};
    validate(result);

    return result;
}
void JsonMusicPreferences::save(const QString& path, const core::MusicPreferences& preferences)
{
    validate(preferences);
    const auto bytes = QJsonDocument{QJsonObject{{"version", kVersion},
                                                 {"folder", QString::fromStdString(preferences.folder)},
                                                 {"volume", preferences.volume}}}
                           .toJson();
    require(bytes.size() <= kMaximumBytes);
    QSaveFile file{path};
    require(file.open(QIODevice::WriteOnly));
    require(file.write(bytes) == bytes.size());
    require(file.commit());
}
} // namespace msa::infra
