#include "msa/mobile/MobileRuntime.h"

#include "msa/infra/JsonClientPreferences.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <exception>
#include <utility>

namespace msa::mobile
{
namespace
{
constexpr int kSettingsPage = 3;
bool sameEndpoint(const core::ClientPreferences& first, const core::ClientPreferences& second)
{
    return first.host == second.host && first.port == second.port && first.token == second.token;
}
} // namespace
MobileRuntime::MobileRuntime(MobileRuntimeOptions options, QObject* parent) :
    QObject{parent}, m_options{std::move(options)}
{
    connect(m_state.settings(), &ui::ClientSettingsModel::saveRequested, this,
            [this](const core::ClientPreferences& preferences) { savePreferences(preferences); });
    connect(m_state.settings(), &ui::ClientSettingsModel::connectRequested, this, [this]() { connectSaved(); });
    connect(&m_models, &ui::ClientModels::reconnectRequested, this, [this]() { connectSaved(); });
}
MobileRuntime::~MobileRuntime()
{
    stop();
}
ui::ClientModels* MobileRuntime::models()
{
    return &m_models;
}
ui::MobileState* MobileRuntime::state()
{
    return &m_state;
}
QString MobileRuntime::defaultSettingsPath()
{
    return QDir{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)}.filePath("client.settings.json");
}
void MobileRuntime::loadPreferences()
{
    try
    {
        const auto preferences = infra::JsonClientPreferences::load(m_options.clientSettingsPath);
        if (preferences)
        {
            m_state.settings()->applyPreferences(*preferences);
            return;
        }
        m_state.settings()->setSettingsMessage("Заполните параметры сервера. До подключения приложение работает без связи.");
    }
    catch (const std::exception&)
    {
        m_state.settings()->setSettingsMessage("Не удалось прочитать настройки. Проверьте файл или сохраните новые параметры.");
    }
    m_state.navigate(kSettingsPage);
}
void MobileRuntime::savePreferences(const core::ClientPreferences& preferences)
{
    try
    {
        if (!QDir{}.mkpath(QFileInfo{m_options.clientSettingsPath}.absolutePath()))
        {
            m_state.settings()->setSettingsMessage("Не удалось создать каталог настроек.");
            return;
        }
        infra::JsonClientPreferences::save(m_options.clientSettingsPath, preferences);
        m_state.settings()->applyPreferences(preferences);
        m_state.settings()->setSettingsMessage("Настройки сохранены. Нажмите «Подключиться», чтобы применить их сейчас.");
    }
    catch (const std::exception&)
    {
        m_state.settings()->setSettingsMessage("Не удалось сохранить настройки. Прежний файл сохранён.");
    }
}
void MobileRuntime::start()
{
    if (m_started || m_stopped)
    {
        return;
    }
    m_started = true;
    loadPreferences();
    if (m_state.settings()->canConnect() && m_state.settings()->autoConnect())
    {
        connectSaved();
    }
}
void MobileRuntime::stopSession()
{
    // Отложенные отказы не переживут удаление транспорта; сначала завершаем ожидавшие записи.
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
void MobileRuntime::stop()
{
    if (m_stopped)
    {
        return;
    }
    m_stopped = true;
    stopSession();
}
void MobileRuntime::connectSaved()
{
    if (!m_started || m_stopped)
    {
        return;
    }
    auto* settings = m_state.settings();
    if (!settings->canConnect())
    {
        m_state.navigate(kSettingsPage);
        settings->setSettingsMessage("Сначала сохраните корректные параметры сервера.");
        return;
    }
    const auto preferences = settings->preferences();
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
} // namespace msa::mobile
