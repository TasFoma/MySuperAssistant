#include "msa/infra/AudioDevices.h"

#include "msa/infra/JsonAudioDevicePreferences.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{
std::vector<core::AudioDeviceChoice> choices(const QList<QAudioDevice>& devices, const QAudioDevice& fallback,
                                             const std::string& selected)
{
    auto name = QString::fromUtf8("По умолчанию");
    if (!fallback.isNull())
    {
        name += " (" + fallback.description() + ")";
    }
    else
    {
        name += " (устройство не найдено)";
    }
    std::vector<core::AudioDeviceChoice> result{{{}, name.toStdString(), true}};
    for (const auto& device : devices)
    {
        result.push_back({device.id().toBase64().toStdString(), device.description().toStdString(), true});
    }
    if (std::ranges::none_of(result, [&selected](const auto& item) { return item.identifier == selected; }))
    {
        result.push_back({selected, "Недоступное устройство — выберите другое", false});
    }

    return result;
}
} // namespace
AudioDevices::AudioDevices(QString settingsPath, QObject* parent) :
    QObject{parent}, m_settingsPath{std::move(settingsPath)}
{
    connect(&m_devices, &QMediaDevices::audioInputsChanged, this, &AudioDevices::refresh);
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, &AudioDevices::refresh);
}
void AudioDevices::restore()
{
    try
    {
        m_preferences = JsonAudioDevicePreferences::load(m_settingsPath).value_or(core::AudioDevicePreferences{});
        m_error.clear();
    }
    catch (const std::exception&)
    {
        m_error = "Не удалось прочитать настройки аудиоустройств. Выберите их заново.";
    }
    refresh();
}
void AudioDevices::refresh()
{
    emit changed();
}
core::AudioDevicesState AudioDevices::state() const
{
    return {choices(QMediaDevices::audioInputs(), QMediaDevices::defaultAudioInput(), m_preferences.input),
            choices(QMediaDevices::audioOutputs(), QMediaDevices::defaultAudioOutput(), m_preferences.output),
            m_preferences, m_error.toStdString()};
}
QAudioDevice AudioDevices::outputDevice() const
{
    if (m_preferences.output.empty())
    {
        return QMediaDevices::defaultAudioOutput();
    }
    const auto devices = QMediaDevices::audioOutputs();
    const auto found = std::ranges::find_if(devices, [this](const QAudioDevice& device)
                                            { return device.id().toBase64().toStdString() == m_preferences.output; });

    return found == devices.end() ? QAudioDevice{} : *found;
}
void AudioDevices::select(bool output, const QString& identifier)
{
    auto preferences = m_preferences;
    auto& selection = output ? preferences.output : preferences.input;
    const auto catalog = state();
    const auto& devices = output ? catalog.outputs : catalog.inputs;
    const auto value = identifier.toStdString();
    if (!JsonAudioDevicePreferences::validIdentifier(identifier) ||
        std::ranges::none_of(devices,
                             [&value](const auto& item) { return item.identifier == value && item.available; }))
    {
        m_error = "Устройство больше недоступно. Обновите список.";
        refresh();
        return;
    }
    selection = value;
    try
    {
        if (!QDir{}.mkpath(QFileInfo{m_settingsPath}.absolutePath()))
        {
            throw std::runtime_error{"Не удалось создать папку настроек."};
        }
        JsonAudioDevicePreferences::save(m_settingsPath, preferences);
        m_preferences = std::move(preferences);
        m_error.clear();
    }
    catch (const std::exception&)
    {
        m_error = "Не удалось сохранить аудиоустройства. Прежний выбор сохранён.";
    }
    refresh();
}
} // namespace msa::infra
