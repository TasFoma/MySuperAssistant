#include "msa/desktop/DesktopRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <exception>
#include <utility>

namespace msa::desktop
{
namespace
{
constexpr int kSettingsPage = 3;
QString voiceSettings(const DesktopRuntimeOptions& options)
{
    return options.voiceSettingsPath.isEmpty()
               ? QFileInfo{options.clientSettingsPath}.absoluteDir().filePath("voice.settings.json")
               : options.voiceSettingsPath;
}
QString voiceWorker(const DesktopRuntimeOptions& options)
{
    return options.voiceWorkerPath.isEmpty()
               ? QDir{QCoreApplication::applicationDirPath()}.filePath(QString::fromUtf8(MSA_VOICE_WORKER_FILE_NAME))
               : options.voiceWorkerPath;
}
bool sameEndpoint(const core::ClientPreferences& first, const core::ClientPreferences& second)
{
    return first.host == second.host && first.port == second.port && first.token == second.token;
}
} // namespace
DesktopRuntime::DesktopRuntime(DesktopRuntimeOptions options, QObject* parent) :
    QObject{parent}, m_options{std::move(options)}, m_preferences{m_state, m_options.clientSettingsPath, this},
    m_player{m_options.musicSettingsPath, m_options.musicSeed, this}, m_music{*m_state.music(), m_player, this},
    m_recognizer{voiceWorker(m_options), {}, this}, m_synthesizer{{}, this},
    m_speech{*m_state.voice(), m_synthesizer, m_recognizer, this},
    m_voice{*m_state.voice(), m_recognizer, voiceSettings(m_options), this}
{
    connect(&m_state, &ui::DesktopState::connectRequested, this, [this]() { connectSaved(); });
    connect(&m_models, &ui::ClientModels::reconnectRequested, this, [this]() { connectSaved(); });
}
DesktopRuntime::~DesktopRuntime()
{
    stop();
}
ui::ClientModels* DesktopRuntime::models()
{
    return &m_models;
}
ui::DesktopState* DesktopRuntime::state()
{
    return &m_state;
}
void DesktopRuntime::start()
{
    if (m_started || m_stopped)
    {
        return;
    }
    m_started = true;
    m_preferences.load();
    m_music.restore();
    m_voice.restore();
    if (m_state.canConnect() && m_state.autoConnect())
    {
        connectSaved();
    }
}
void DesktopRuntime::stopSession()
{
    // Отложенные отказы транспорта исчезнут вместе с ним; сначала завершаем запросы связки.
    if (m_controller)
    {
        m_controller->stop();
    }
    else if (m_client)
    {
        m_client->disconnectFromServer();
    }
    m_controller.reset();
    m_client.reset();
}
void DesktopRuntime::stop()
{
    if (m_stopped)
    {
        return;
    }
    m_stopped = true;
    m_synthesizer.stop();
    m_recognizer.shutdown();
    stopSession();
}
void DesktopRuntime::connectSaved()
{
    if (!m_started || m_stopped)
    {
        return;
    }
    if (!m_state.canConnect())
    {
        m_state.navigate(kSettingsPage);
        m_state.setSettingsMessage("Сначала сохраните корректные параметры сервера.");
        return;
    }
    const auto preferences = m_state.preferences();
    if (m_client && m_activePreferences && sameEndpoint(*m_activePreferences, preferences))
    {
        m_client->reconnect();
        return;
    }
    stopSession();
    if (m_activePreferences && !sameEndpoint(*m_activePreferences, preferences))
    {
        const auto unknown = m_models.errorEffect() == "unknown";
        const auto previousError = m_models.errorText();
        m_models.resetServerData();
        if (unknown)
        {
            m_models.setError(previousError, "unknown");
        }
    }
    try
    {
        m_client.emplace(infra::TcpClientSettings{QString::fromStdString(preferences.host), preferences.port,
                                                  QString::fromStdString(preferences.token), m_options.connectTimeout,
                                                  m_options.responseTimeout, m_options.initialReconnectDelay,
                                                  m_options.maximumReconnectDelay});
        m_controller.emplace(*m_client, m_models, this);
        m_activePreferences = preferences;
        m_client->connectToServer();
    }
    catch (const std::exception&)
    {
        stopSession();
        m_models.setError("Не удалось создать подключение. Проверьте сохранённые параметры.");
    }
}
} // namespace msa::desktop
