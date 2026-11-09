#include "msa/desktop/SpeechController.h"

namespace msa::desktop
{
SpeechController::SpeechController(ui::VoiceModel& model, core::SpeechSynthesizer& synthesizer,
                                   core::SpeechRecognizer& recognizer, QObject* parent) :
    QObject{parent}, m_synthesizer{synthesizer}
{
    synthesizer.setHandler([&model](const core::SpeechEvent& event) { model.applySpeechEvent(event); });
    connect(&model, &ui::VoiceModel::speechRequested, this,
            [&synthesizer, &recognizer](const QString& text)
            {
                const auto state = recognizer.state();
                if (state == core::RecognitionState::Loading || state == core::RecognitionState::Recording ||
                    state == core::RecognitionState::Finishing)
                {
                    recognizer.cancel();
                }
                synthesizer.speak(text.toStdString());
            });
    connect(&model, &ui::VoiceModel::stopSpeechRequested, this, [&synthesizer] { synthesizer.stop(); });
    // Подключение создаётся до VoiceController, поэтому stop выполняется до начала записи.
    connect(&model, &ui::VoiceModel::commandRequested, this,
            [&synthesizer](core::RecognitionCommand command)
            {
                if (command == core::RecognitionCommand::Start)
                {
                    synthesizer.stop();
                }
            });
}
SpeechController::~SpeechController()
{
    m_synthesizer.setHandler({});
    m_synthesizer.stop();
}
} // namespace msa::desktop
