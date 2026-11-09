#pragma once

#include "msa/core/SpeechRecognizer.h"
#include "msa/core/SpeechSynthesizer.h"
#include "msa/ui/VoiceModel.h"

#include <QObject>

namespace msa::desktop
{
/// @brief Соединяет озвучку с UI и исключает одновременную запись собственного голоса.
class SpeechController final : public QObject
{
public:
    /// @brief Все переданные объекты должны пережить контроллер; конструктор не включает звук.
    SpeechController(ui::VoiceModel& model, core::SpeechSynthesizer& synthesizer, core::SpeechRecognizer& recognizer,
                     QObject* parent = nullptr);
    ~SpeechController() override;

private:
    core::SpeechSynthesizer& m_synthesizer;
};
} // namespace msa::desktop
