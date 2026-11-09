#include "msa/infra/JsonAudioDevicePreferences.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr qint64 kMaximumBytes = 16384;
constexpr qsizetype kMaximumIdentifier = 4096;
constexpr int kVersion = 1;
void require(bool valid)
{
    if (!valid)
    {
        throw std::runtime_error{"Некорректный файл настроек аудиоустройств."};
    }
}
} // namespace
bool JsonAudioDevicePreferences::validIdentifier(const QString& identifier)
{
    const auto bytes = identifier.toLatin1();
    const auto decoded = QByteArray::fromBase64Encoding(bytes, QByteArray::AbortOnBase64DecodingErrors);

    return identifier.size() <= kMaximumIdentifier && QString::fromLatin1(bytes) == identifier && decoded &&
           decoded.decoded.toBase64() == bytes;
}
std::optional<core::AudioDevicePreferences> JsonAudioDevicePreferences::load(const QString& path)
{
    if (!QFileInfo::exists(path))
    {
        return std::nullopt;
    }
    QFile file{path};
    require(file.open(QIODevice::ReadOnly) && file.size() <= kMaximumBytes);
    const auto document = QJsonDocument::fromJson(file.read(kMaximumBytes + 1));
    require(file.error() == QFileDevice::NoError && document.isObject());
    const auto object = document.object();
    require(object.size() == 3 && object.value("version").isDouble() &&
            object.value("version").toDouble() == kVersion && object.value("input_device").isString() &&
            object.value("output_device").isString());
    const auto input = object.value("input_device").toString();
    const auto output = object.value("output_device").toString();
    require(validIdentifier(input) && validIdentifier(output));

    return core::AudioDevicePreferences{input.toStdString(), output.toStdString()};
}
void JsonAudioDevicePreferences::save(const QString& path, const core::AudioDevicePreferences& preferences)
{
    const auto input = QString::fromStdString(preferences.input);
    const auto output = QString::fromStdString(preferences.output);
    require(validIdentifier(input) && validIdentifier(output));
    const auto bytes =
        QJsonDocument{QJsonObject{{"version", kVersion}, {"input_device", input}, {"output_device", output}}}.toJson();
    QSaveFile file{path};
    require(bytes.size() <= kMaximumBytes && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() &&
            file.commit());
}
} // namespace msa::infra
