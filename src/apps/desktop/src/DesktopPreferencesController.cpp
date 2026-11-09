#include "DesktopPreferencesController.h"

#include "msa/infra/JsonClientPreferences.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <exception>
#include <utility>

namespace msa::desktop
{
namespace
{
constexpr int kSettingsPage = 3;
}
DesktopPreferencesController::DesktopPreferencesController(ui::DesktopState& state, QString path, QObject* parent) :
    QObject{parent}, m_state{state}, m_path{std::move(path)}
{
    connect(&state, &ui::DesktopState::saveRequested, this,
            [this](const core::ClientPreferences& preferences)
            {
                try
                {
                    if (!QDir{}.mkpath(QFileInfo{m_path}.absolutePath()))
                    {
                        m_state.setSettingsMessage("Не удалось создать каталог настроек.");
                        return;
                    }
                    infra::JsonClientPreferences::save(m_path, preferences);
                    m_state.applyPreferences(preferences);
                    m_state.setSettingsMessage(
                        "Настройки сохранены. Новое подключение использует их при запуске клиента.");
                }
                catch (const std::exception&)
                {
                    m_state.setSettingsMessage("Не удалось сохранить настройки. Прежний файл сохранён.");
                }
            });
}
QString DesktopPreferencesController::defaultPath()
{
    return QDir{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)}.filePath("client.settings.json");
}
void DesktopPreferencesController::load()
{
    try
    {
        const auto preferences = infra::JsonClientPreferences::load(m_path);
        if (preferences)
        {
            m_state.applyPreferences(*preferences);
            return;
        }
        m_state.setSettingsMessage("Заполните параметры сервера. До подключения приложение работает без связи.");
    }
    catch (const std::exception&)
    {
        m_state.setSettingsMessage("Не удалось прочитать настройки. Проверьте файл или сохраните новые параметры.");
    }
    m_state.navigate(kSettingsPage);
}
} // namespace msa::desktop
