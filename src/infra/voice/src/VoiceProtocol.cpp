#include "msa/infra/VoiceProtocol.h"

#include <QJsonDocument>

#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr int kVersion = 1;
void require(bool valid)
{
    if (!valid)
    {
        throw std::runtime_error{"Неверный ответ процесса распознавания."};
    }
}
} // namespace
QByteArray VoiceProtocol::encode(QJsonObject object)
{
    object.insert("version", kVersion);
    const auto bytes = QJsonDocument{object}.toJson(QJsonDocument::Compact);
    require(bytes.size() <= kMaximumFrameBytes);

    return bytes + '\n';
}
QList<QJsonObject> VoiceProtocol::consume(QByteArray& pending, const QByteArray& bytes)
{
    require(bytes.size() <= kMaximumFrameBytes && pending.size() <= kMaximumFrameBytes);
    pending += bytes;
    QList<QJsonObject> result;
    while (true)
    {
        const auto newline = pending.indexOf('\n');
        if (newline < 0)
        {
            break;
        }
        require(newline <= kMaximumFrameBytes);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(pending.first(newline), &error);
        require(error.error == QJsonParseError::NoError && document.isObject());
        const auto object = document.object();
        require(object.value("version").isDouble() && object.value("version").toDouble() == kVersion &&
                object.value("type").isString());
        result.append(object);
        pending.remove(0, newline + 1);
    }
    require(pending.size() <= kMaximumFrameBytes);

    return result;
}
quint64 VoiceProtocol::session(const QJsonObject& object)
{
    const auto value = object.value("session");
    require(value.isString());
    bool valid{false};
    const auto result = value.toString().toULongLong(&valid);
    require(valid && result > 0 && QString::number(result) == value.toString());

    return result;
}
} // namespace msa::infra
