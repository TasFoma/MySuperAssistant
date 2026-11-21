#include "msa/ui/DesktopState.h"

#include <QDate>
#include <QLocale>

#include <algorithm>

namespace msa::ui
{
namespace
{

constexpr int kDateRefreshMilliseconds = 30000;
[[nodiscard]] QString currentDateText()
{
    return QLocale{QLocale::Russian}.toString(QDate::currentDate(), QStringLiteral("dddd, d MMMM yyyy"));
}
} // namespace
DesktopState::DesktopState(ClientModels& models, QObject* parent) : QObject{parent}, m_models{models}
{
    m_todayDate = currentDateText();
    m_dateTimer.setInterval(kDateRefreshMilliseconds);
    connect(&m_dateTimer, &QTimer::timeout, this,
            [this]
            {
                const auto date = currentDateText();
                if (date != m_todayDate)
                {
                    m_todayDate = date;
                    emit todayDateChanged();
                }
            });
    m_dateTimer.start();
    connect(&m_settings, &ClientSettingsModel::preferencesChanged, this, &DesktopState::preferencesChanged);
    connect(&m_settings, &ClientSettingsModel::settingsMessageChanged, this, &DesktopState::settingsMessageChanged);
    connect(&m_settings, &ClientSettingsModel::saveRequested, this, &DesktopState::saveRequested);
    connect(&m_settings, &ClientSettingsModel::connectRequested, this, &DesktopState::connectRequested);
}
DesktopState::Page DesktopState::page() const
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
AudioDevicesModel* DesktopState::audioDevices()
{
    return &m_audioDevices;
}
QStringList DesktopState::pages() const
{
    QStringList titles;
    for (const auto& section : sections())
    {
        titles.append(section.toMap().value("title").toString());
    }

    return titles;
}
QVariantList DesktopState::sections() const
{
    return {
        QVariantMap{
            {"section", QVariant::fromValue(Page::Today)}, {"key", "today"}, {"title", "Сегодня"}, {"icon", "today"}},
        QVariantMap{
            {"section", QVariant::fromValue(Page::Plans)}, {"key", "plans"}, {"title", "Планы"}, {"icon", "plans"}},
        QVariantMap{{"section", QVariant::fromValue(Page::Workouts)},
                    {"key", "workouts"},
                    {"title", "Тренировки"},
                    {"icon", "workouts"}},
        QVariantMap{
            {"section", QVariant::fromValue(Page::Music)}, {"key", "music"}, {"title", "Музыка"}, {"icon", "music"}},
        QVariantMap{{"section", QVariant::fromValue(Page::Assistant)},
                    {"key", "assistant"},
                    {"title", "Ассистент"},
                    {"icon", "assistant"}},
        QVariantMap{{"section", QVariant::fromValue(Page::Settings)},
                    {"key", "settings"},
                    {"title", "Настройки"},
                    {"icon", "settings"}}};
}
QVariantList DesktopState::planSections() const
{
    return {
        QVariantMap{{"section", QVariant::fromValue(PlanSection::Tasks)}, {"key", "tasks"}, {"title", "Задачи"}},
        QVariantMap{
            {"section", QVariant::fromValue(PlanSection::Calendar)}, {"key", "calendar"}, {"title", "Календарь"}},
        QVariantMap{{"section", QVariant::fromValue(PlanSection::Habits)}, {"key", "habits"}, {"title", "Привычки"}},
        QVariantMap{
            {"section", QVariant::fromValue(PlanSection::Reminders)}, {"key", "reminders"}, {"title", "Напоминания"}}};
}
DesktopState::PlanSection DesktopState::planSection() const
{
    return m_planSection;
}
QString DesktopState::todayDate() const
{
    return m_todayDate;
}
void DesktopState::selectPlanSection(PlanSection section)
{
    const auto choices = planSections();
    if (m_planSection == section ||
        !std::ranges::any_of(choices, [section](const QVariant& choice)
                             { return choice.toMap().value("section").value<PlanSection>() == section; }))
    {
        return;
    }
    m_planSection = section;
    emit planSectionChanged();
}
QString DesktopState::host() const
{
    return m_settings.host();
}
QString DesktopState::port() const
{
    return m_settings.port();
}
QString DesktopState::token() const
{
    return m_settings.token();
}
bool DesktopState::autoConnect() const
{
    return m_settings.autoConnect();
}
core::ClientPreferences DesktopState::preferences() const
{
    return m_settings.preferences();
}
bool DesktopState::canConnect() const
{
    return m_settings.canConnect();
}
void DesktopState::connectToServer()
{
    m_settings.connectToServer();
}
QString DesktopState::settingsMessage() const
{
    return m_settings.settingsMessage();
}
void DesktopState::applyPreferences(const core::ClientPreferences& preferences)
{
    m_settings.applyPreferences(preferences);
}
void DesktopState::setSettingsMessage(const QString& message)
{
    m_settings.setSettingsMessage(message);
}
void DesktopState::navigate(Page page)
{
    const auto choices = sections();
    if (m_page == page || !std::ranges::any_of(choices, [page](const QVariant& choice)
                                               { return choice.toMap().value("section").value<Page>() == page; }))
    {
        return;
    }
    m_page = page;
    emit pageChanged();
}
bool DesktopState::savePreferences(const QString& host, const QString& port, const QString& token, bool autoConnect)
{
    return m_settings.savePreferences(host, port, token, autoConnect);
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
