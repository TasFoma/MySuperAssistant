#include "msa/ui/DesktopState.h"

#include <QDate>
#include <algorithm>

namespace msa::ui
{
namespace
{
constexpr int kMaximumPort = 65535;
constexpr int kPageCount = 4;
} // namespace
DesktopState::DesktopState(ClientModels& models, QObject* parent) : QObject{parent}, m_models{models} {}
int DesktopState::page() const
{
    return m_page;
}
PlannerEditor* DesktopState::planner()
{
    return &m_planner;
}
MusicModel* DesktopState::music()
{
    return &m_music;
}
VoiceModel* DesktopState::voice()
{
    return &m_voice;
}
QStringList DesktopState::pages() const
{
    return {"Чат", "Планы", "Напоминания", "Настройки"};
}
QString DesktopState::host() const
{
    return QString::fromStdString(m_preferences.host);
}
QString DesktopState::port() const
{
    return m_preferences.port == 0 ? QString{} : QString::number(m_preferences.port);
}
QString DesktopState::token() const
{
    return QString::fromStdString(m_preferences.token);
}
bool DesktopState::autoConnect() const
{
    return m_preferences.autoConnect;
}
core::ClientPreferences DesktopState::preferences() const
{
    return m_preferences;
}
bool DesktopState::canConnect() const
{
    return core::validClientPreferences(m_preferences) && !token().trimmed().isEmpty();
}
void DesktopState::connectToServer()
{
    if (!canConnect())
    {
        setSettingsMessage("Сначала сохраните корректные параметры сервера.");
        return;
    }
    emit connectRequested();
}
QString DesktopState::settingsMessage() const
{
    return m_settingsMessage;
}
void DesktopState::applyPreferences(const core::ClientPreferences& preferences)
{
    m_preferences = preferences;
    emit preferencesChanged();
}
void DesktopState::setSettingsMessage(const QString& message)
{
    m_settingsMessage = message;
    emit settingsMessageChanged();
}
void DesktopState::navigate(int page)
{
    if (page < 0 || page >= kPageCount || m_page == page)
    {
        return;
    }
    m_page = page;
    emit pageChanged();
}
bool DesktopState::savePreferences(const QString& host, const QString& port, const QString& token, bool autoConnect)
{
    bool validPort{false};
    const auto number = port.toUInt(&validPort);
    if (!validPort || number == 0 || number > kMaximumPort || port.isEmpty() ||
        !std::ranges::all_of(port, [](QChar character) { return character >= '0' && character <= '9'; }))
    {
        setSettingsMessage("Порт должен быть целым числом от 1 до 65535.");
        return false;
    }
    const core::ClientPreferences preferences{host.trimmed().toStdString(), static_cast<std::uint16_t>(number),
                                              token.toStdString(), autoConnect};
    if (!core::validClientPreferences(preferences) || token.trimmed().isEmpty())
    {
        setSettingsMessage("Укажите хост без схемы URL и непустой токен.");
        return false;
    }
    emit saveRequested(preferences);
    return true;
}
bool DesktopState::showCalendarDay(const QString& date)
{
    const auto parsed = QDate::fromString(date, Qt::ISODate);
    if (!parsed.isValid() || parsed.toString(Qt::ISODate) != date)
    {
        m_models.setError("Введите дату в формате ГГГГ-ММ-ДД.");
        return false;
    }
    return m_models.showDay(date);
}
} // namespace msa::ui
