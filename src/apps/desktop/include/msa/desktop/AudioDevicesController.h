#pragma once

#include "msa/infra/AudioDevices.h"
#include "msa/infra/LocalMusicPlayer.h"
#include "msa/infra/ProcessSpeechRecognizer.h"
#include "msa/infra/QtSpeechSynthesizer.h"
#include "msa/ui/AudioDevicesModel.h"

#include <QObject>

namespace msa::desktop
{
/// @brief Применяет сохранённые устройства к музыке, диктовке и озвучке, не открывая запись.
class AudioDevicesController final : public QObject
{
public:
    AudioDevicesController(ui::AudioDevicesModel& model, infra::AudioDevices& devices, infra::LocalMusicPlayer& music,
                           infra::ProcessSpeechRecognizer& recognizer, infra::QtSpeechSynthesizer& synthesizer,
                           QObject* parent = nullptr);
};
} // namespace msa::desktop
