#include "msa/infra/SpeechEngineFactory.h"

#include <stdexcept>

namespace msa::infra
{
std::unique_ptr<QTextToSpeech> createSpeechEngine(const QString& engine, [[maybe_unused]] const QAudioDevice& output,
                                                  bool systemDefault)
{
    if (!systemDefault)
    {
        throw std::runtime_error{"Выбор вывода озвучки на этой платформе пока не поддерживается."};
    }

    return std::make_unique<QTextToSpeech>(engine);
}
} // namespace msa::infra
