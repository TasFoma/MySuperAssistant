#include "msa/ui/AudioDevicesModel.h"

#include <algorithm>

namespace msa::ui
{
namespace
{
QStringList names(const std::vector<core::AudioDeviceChoice>& devices)
{
    QStringList result;
    for (const auto& device : devices)
    {
        result.push_back(QString::fromStdString(device.name));
    }

    return result;
}
int selectedIndex(const std::vector<core::AudioDeviceChoice>& devices, const std::string& identifier)
{
    const auto found =
        std::ranges::find_if(devices, [&identifier](const auto& device) { return device.identifier == identifier; });

    return found == devices.end() ? -1 : static_cast<int>(found - devices.begin());
}
} // namespace
AudioDevicesModel::AudioDevicesModel(QObject* parent) : QObject{parent} {}
void AudioDevicesModel::applyState(const core::AudioDevicesState& state)
{
    m_state = state;
    emit changed();
}
QStringList AudioDevicesModel::inputNames() const
{
    return names(m_state.inputs);
}
QStringList AudioDevicesModel::outputNames() const
{
    return names(m_state.outputs);
}
int AudioDevicesModel::inputIndex() const
{
    return selectedIndex(m_state.inputs, m_state.preferences.input);
}
int AudioDevicesModel::outputIndex() const
{
    return selectedIndex(m_state.outputs, m_state.preferences.output);
}
QString AudioDevicesModel::errorText() const
{
    return QString::fromStdString(m_state.error);
}
void AudioDevicesModel::selectInput(int index)
{
    select(false, index);
}
void AudioDevicesModel::selectOutput(int index)
{
    select(true, index);
}
void AudioDevicesModel::select(bool output, int index)
{
    const auto& devices = output ? m_state.outputs : m_state.inputs;
    if (index < 0 || static_cast<std::size_t>(index) >= devices.size() ||
        !devices[static_cast<std::size_t>(index)].available)
    {
        return;
    }
    emit selectionRequested(output, QString::fromStdString(devices[static_cast<std::size_t>(index)].identifier));
}
void AudioDevicesModel::refresh()
{
    emit refreshRequested();
}
} // namespace msa::ui
