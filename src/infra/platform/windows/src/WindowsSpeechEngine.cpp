#include "msa/infra/SpeechEngineFactory.h"

#include <QVariant>
#include <QVariantMap>

#include <stdexcept>

namespace msa::infra
{
std::unique_ptr<QTextToSpeech> createSpeechEngine(const QString& engine, const QAudioDevice& output, bool systemDefault)
{
    if (systemDefault)
    {
        return std::make_unique<QTextToSpeech>(engine);
    }
    if (!QTextToSpeech::availableEngines().contains("winrt"))
    {
        throw std::runtime_error{"Для выбранного вывода озвучки требуется движок Qt WinRT."};
    }

    return std::make_unique<QTextToSpeech>(QStringLiteral("winrt"),
                                           QVariantMap{{"audioDevice", QVariant::fromValue(output)}});
}
} // namespace msa::infra
