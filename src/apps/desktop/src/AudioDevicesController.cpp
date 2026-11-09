#include "msa/desktop/AudioDevicesController.h"

namespace msa::desktop
{
AudioDevicesController::AudioDevicesController(ui::AudioDevicesModel& model, infra::AudioDevices& devices,
                                               infra::LocalMusicPlayer& music,
                                               infra::ProcessSpeechRecognizer& recognizer,
                                               infra::QtSpeechSynthesizer& synthesizer, QObject* parent) :
    QObject{parent}
{
    connect(&devices, &infra::AudioDevices::changed, this,
            [&model, &devices, &music, &recognizer, &synthesizer]
            {
                const auto state = devices.state();
                model.applyState(state);
                recognizer.setInputDevice(QString::fromStdString(state.preferences.input));
                const auto output = devices.outputDevice();
                music.setOutputDevice(output);
                synthesizer.setOutputDevice(output, state.preferences.output.empty());
            });
    connect(&model, &ui::AudioDevicesModel::selectionRequested, this,
            [&devices](bool output, const QString& identifier) { devices.select(output, identifier); });
    connect(&model, &ui::AudioDevicesModel::refreshRequested, this, [&devices] { devices.refresh(); });
}
} // namespace msa::desktop
