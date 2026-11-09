#include "msa/infra/VoskDecoder.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr float kSampleRate = 16000;
constexpr qsizetype kBytesPerSample = 2;
} // namespace
VoskDecoder::VoskDecoder(const QString& modelPath)
{
    m_model.reset(vosk_model_new(modelPath.toUtf8().constData()));
    if (!m_model)
    {
        throw std::runtime_error{"Не удалось загрузить модель Vosk. Проверьте выбранную папку."};
    }
}
void VoskDecoder::begin()
{
    m_recognizer.reset(vosk_recognizer_new(m_model.get(), kSampleRate));
    if (!m_recognizer)
    {
        throw std::runtime_error{"Не удалось создать распознаватель Vosk."};
    }
}
QString VoskDecoder::text(const char* json)
{
    if (!json)
    {
        throw std::runtime_error{"Vosk не вернул результат."};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray{json}, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() || !document.object().value("text").isString())
    {
        throw std::runtime_error{"Vosk вернул неверный результат."};
    }

    return document.object().value("text").toString().trimmed();
}
std::optional<QString> VoskDecoder::accept(const QByteArray& pcm)
{
    if (!m_recognizer || pcm.size() % kBytesPerSample != 0 || pcm.size() > std::numeric_limits<int>::max())
    {
        throw std::runtime_error{"Неверные данные записи."};
    }
    const auto accepted =
        vosk_recognizer_accept_waveform(m_recognizer.get(), pcm.constData(), static_cast<int>(pcm.size()));
    if (accepted < 0)
    {
        throw std::runtime_error{"Ошибка распознавания Vosk."};
    }
    if (accepted == 0)
    {
        return std::nullopt;
    }
    const auto result = text(vosk_recognizer_result(m_recognizer.get()));
    if (result.isEmpty())
    {
        return std::nullopt;
    }

    return result;
}
QString VoskDecoder::finish()
{
    if (!m_recognizer)
    {
        return {};
    }
    const auto result = text(vosk_recognizer_final_result(m_recognizer.get()));
    m_recognizer.reset();

    return result;
}
} // namespace msa::infra
