#include "msa/ui/MobileState.h"

#include <QLocale>
#include <QVariantMap>

namespace msa::ui
{
namespace
{
constexpr int kPageCount = 4;
constexpr int kDaysPerWeek = 7;
constexpr int kMinimumYear = 1;
constexpr int kMaximumYear = 9999;
constexpr int kIsoDateLength = 10;
} // namespace
MobileState::MobileState(ClientModels& models, QObject* parent) : QObject{parent}, m_models{models}
{
    connect(&models, &ClientModels::metadataChanged, this, &MobileState::updateServerDate);
    connect(&models, &ClientModels::connectionChanged, this, &MobileState::refreshCalendar);
    connect(&models, &ClientModels::calendarChanged, this,
            [this]()
            {
                m_monthLoaded = m_month.isValid() && m_models.online();
                emit calendarChanged();
            });
    updateServerDate();
}
int MobileState::page() const
{
    return m_page;
}
QStringList MobileState::pages() const
{
    return {"Чат", "Планы", "Напоминания", "Настройки"};
}
bool MobileState::conversationsVisible() const
{
    return m_conversationsVisible;
}
QString MobileState::draft() const
{
    return m_draft;
}
QString MobileState::searchQuery() const
{
    return m_searchQuery;
}
QString MobileState::monthTitle() const
{
    return m_month.isValid() ? QLocale{QLocale::Russian}.toString(m_month, "MMMM yyyy") : QString{"Нет даты сервера"};
}
QString MobileState::selectedDate() const
{
    return m_selectedDate.toString(Qt::ISODate);
}
PlannerEditor* MobileState::planner()
{
    return &m_planner;
}
ClientSettingsModel* MobileState::settings()
{
    return &m_settings;
}
void MobileState::setDraft(const QString& text)
{
    if (m_draft == text)
    {
        return;
    }
    m_draft = text;
    emit draftChanged();
}
void MobileState::setSearchQuery(const QString& text)
{
    if (m_searchQuery == text)
    {
        return;
    }
    m_searchQuery = text;
    emit draftChanged();
}
void MobileState::navigate(int page)
{
    if (page < 0 || page >= kPageCount || page == m_page)
    {
        return;
    }
    m_page = page;
    emit navigationChanged();
}
void MobileState::showConversations()
{
    m_conversationsVisible = true;
    emit navigationChanged();
}
bool MobileState::selectConversation(const QString& identifier)
{
    if (!m_models.selectChat(identifier))
    {
        return false;
    }
    m_conversationsVisible = false;
    emit navigationChanged();

    return true;
}
bool MobileState::goBack()
{
    if (m_planner.active())
    {
        m_planner.cancel();
        return true;
    }
    if (m_page != 0)
    {
        navigate(0);
        return true;
    }
    if (!m_conversationsVisible)
    {
        return false;
    }
    m_conversationsVisible = false;
    emit navigationChanged();

    return true;
}
bool MobileState::sendMessage()
{
    if (!m_models.sendMessage(m_draft, m_searchQuery))
    {
        return false;
    }
    m_retrySearchQuery = m_searchQuery;
    m_retryChat = m_models.selectedChatIdentifier();
    // Черновик сохраняется: принятие действия UI ещё не подтверждает сохранение User сервером.
    return true;
}
bool MobileState::retryMessage()
{
    if (m_models.errorEffect() != "user_saved" || m_retryChat != m_models.selectedChatIdentifier())
    {
        return false;
    }

    return m_models.retryMessage(m_models.savedUserIdentifier(), m_retrySearchQuery);
}
QVariantList MobileState::calendarCells() const
{
    QVariantList cells;
    if (!m_month.isValid())
    {
        return cells;
    }
    const auto offset = m_month.dayOfWeek() - 1;
    const auto count = ((offset + m_month.daysInMonth() + kDaysPerWeek - 1) / kDaysPerWeek) * kDaysPerWeek;
    const auto eventDays = m_models.monthDays();
    for (int index{0}; index < count; ++index)
    {
        const auto day = index - offset + 1;
        const auto valid = day > 0 && day <= m_month.daysInMonth();
        const auto date = valid ? QDate{m_month.year(), m_month.month(), day} : QDate{};
        cells.append(QVariantMap{{"label", valid ? QString::number(day) : QString{}},
                                 {"date", date.toString(Qt::ISODate)},
                                 {"valid", valid},
                                 {"selected", valid && date == m_selectedDate},
                                 {"today", valid && date == m_today},
                                 {"hasEvents", valid && m_monthLoaded && eventDays.contains(day)}});
    }

    return cells;
}
void MobileState::updateServerDate()
{
    m_today = QDate::fromString(m_models.serverTime().left(kIsoDateLength), Qt::ISODate);
    if (!m_today.isValid())
    {
        m_month = {};
        m_selectedDate = {};
        m_monthLoaded = false;
        m_retrySearchQuery.clear();
        m_retryChat.clear();
    }
    else if (!m_month.isValid())
    {
        m_month = QDate{m_today.year(), m_today.month(), 1};
        m_selectedDate = m_today;
        refreshCalendar();
    }
    emit calendarChanged();
}
void MobileState::refreshCalendar()
{
    if (!m_models.online() || !m_month.isValid())
    {
        return;
    }
    static_cast<void>(m_models.showMonth(m_month.year(), m_month.month()));
    static_cast<void>(m_models.showDay(selectedDate()));
}
void MobileState::shiftMonth(int offset)
{
    if (!m_month.isValid() || (offset != -1 && offset != 1))
    {
        return;
    }
    const auto next = m_month.addMonths(offset);
    if (!next.isValid() || next.year() < kMinimumYear || next.year() > kMaximumYear)
    {
        return;
    }
    m_month = next;
    m_selectedDate = next;
    m_monthLoaded = false;
    refreshCalendar();
    emit calendarChanged();
}
bool MobileState::selectDay(const QString& text)
{
    const auto date = QDate::fromString(text, Qt::ISODate);
    if (!date.isValid() || date.toString(Qt::ISODate) != text || date.year() != m_month.year() ||
        date.month() != m_month.month())
    {
        return false;
    }
    m_selectedDate = date;
    static_cast<void>(m_models.showDay(text));
    emit calendarChanged();

    return true;
}
} // namespace msa::ui
