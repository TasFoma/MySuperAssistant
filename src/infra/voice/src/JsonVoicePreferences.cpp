#include "msa/infra/JsonVoicePreferences.h"

#include <QDir>
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
constexpr qint64 kMaximumBytes = 65536;
constexpr int kVersion = 1;
void require(bool valid)
{
    if (!valid)
    {
        throw std::runtime_error{"Не удалось прочитать или сохранить параметры голоса."};
    }
}
void validate(const QString& path)
{
    require(!path.contains(QChar::Null) && (path.isEmpty() || QDir::isAbsolutePath(path)));
}
} // namespace
std::optional<QString> JsonVoicePreferences::load(const QString& path)
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
    require(object.size() == 2 && object.value("version").isDouble() &&
            object.value("version").toDouble() == kVersion && object.value("model_path").isString());
    const auto result = object.value("model_path").toString();
    validate(result);

    return result;
}
void JsonVoicePreferences::save(const QString& path, const QString& modelPath)
{
    validate(modelPath);
    const auto bytes = QJsonDocument{QJsonObject{{"version", kVersion}, {"model_path", modelPath}}}.toJson();
    require(bytes.size() <= kMaximumBytes);
    QSaveFile file{path};
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit());
}
} // namespace msa::infra
