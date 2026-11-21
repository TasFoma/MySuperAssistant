#include "msa/ui/ClientSettingsModel.h"

#include <algorithm>

namespace msa::ui
{
namespace
{
constexpr int kMaximumPort = 65535;
} // namespace
ClientSettingsModel::ClientSettingsModel(QObject* parent) : QObject{parent} {}
QString ClientSettingsModel::host() const
{
    return QString::fromStdString(m_preferences.host);
}
QString ClientSettingsModel::port() const
{
    return m_preferences.port == 0 ? QString{} : QString::number(m_preferences.port);
}
QString ClientSettingsModel::token() const
{
    return QString::fromStdString(m_preferences.token);
}
bool ClientSettingsModel::autoConnect() const
{
    return m_preferences.autoConnect;
}
core::ClientPreferences ClientSettingsModel::preferences() const
{
    return m_preferences;
}
bool ClientSettingsModel::canConnect() const
{
    return core::validClientPreferences(m_preferences) && !token().trimmed().isEmpty();
}
void ClientSettingsModel::connectToServer()
{
    if (!canConnect())
    {
        setSettingsMessage("Сначала сохраните корректные параметры сервера.");
        return;
    }
    emit connectRequested();
}
QString ClientSettingsModel::settingsMessage() const
{
    return m_settingsMessage;
}
void ClientSettingsModel::applyPreferences(const core::ClientPreferences& preferences)
{
    m_preferences = preferences;
    emit preferencesChanged();
}
void ClientSettingsModel::setSettingsMessage(const QString& message)
{
    m_settingsMessage = message;
    emit settingsMessageChanged();
}
bool ClientSettingsModel::savePreferences(const QString& host, const QString& port, const QString& token,
                                          bool autoConnect)
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
} // namespace msa::ui
