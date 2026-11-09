#include "msa/ui/ClientModels.h"

#include <QDate>
#include <algorithm>
#include <format>
#include <limits>
#include <set>
#include <type_traits>

namespace msa::ui
{
namespace
{
std::optional<std::int64_t> parsedIdentifier(const QString& text)
{
    bool valid{false};
    const auto value = text.toLongLong(&valid);
    if (!valid || value <= 0 || QString::number(value) != text)
    {
        return std::nullopt;
    }
    return value;
}
template <typename ItemType> std::vector<ClientListRow> rows(const std::vector<ItemType>& values)
{
    std::vector<ClientListRow> result;
    result.reserve(values.size());
    for (const auto& value : values)
    {
        if constexpr (std::is_same_v<ItemType, core::Reminder>)
        {
            result.push_back(std::visit([](const auto& item) { return ClientListRow{item}; }, value));
        }
        else
        {
            result.emplace_back(value);
        }
    }
    return result;
}
} // namespace

ClientModels::ClientModels(QObject* parent) : QObject{parent}
{
    qRegisterMetaType<core::ClientReadRequest>();
    qRegisterMetaType<core::ClientAction>();
}
ClientListModel* ClientModels::tasks()
{
    return &m_tasks;
}
ClientListModel* ClientModels::reminders()
{
    return &m_reminders;
}
ClientListModel* ClientModels::habits()
{
    return &m_habits;
}
ClientListModel* ClientModels::eventsToday()
{
    return &m_eventsToday;
}
ClientListModel* ClientModels::calendarEvents()
{
    return &m_calendarEvents;
}
ClientListModel* ClientModels::chats()
{
    return &m_chats;
}
ClientListModel* ClientModels::messages()
{
    return &m_messages;
}
ClientListModel* ClientModels::profile()
{
    return &m_profile;
}
ClientListModel* ClientModels::moods()
{
    return &m_moods;
}
ClientListModel* ClientModels::routes()
{
    return &m_routes;
}
bool ClientModels::online() const
{
    return m_state == core::ClientConnectionState::Ready && m_hasSnapshot;
}
bool ClientModels::reconnectAvailable() const
{
    return m_state == core::ClientConnectionState::Disconnected || m_state == core::ClientConnectionState::Exhausted;
}
QString ClientModels::mainChatIdentifier() const
{
    return m_mainChat > 0 ? QString::number(m_mainChat) : QString{};
}
QString ClientModels::selectedChatIdentifier() const
{
    return m_selectedChat > 0 ? QString::number(m_selectedChat) : QString{};
}
QString ClientModels::timeZone() const
{
    return QString::fromStdString(m_timeZone);
}
QString ClientModels::serverTime() const
{
    return m_mainChat > 0 ? QString::fromStdString(std::format("{:%FT%T}", m_serverTime)) : QString{};
}
QString ClientModels::errorText() const
{
    return m_error;
}
QString ClientModels::errorEffect() const
{
    return m_effect;
}
QString ClientModels::savedUserIdentifier() const
{
    return m_savedUser;
}
QVariantList ClientModels::monthDays() const
{
    return m_monthDays;
}
QVariantMap ClientModels::routeStatistics() const
{
    return m_routeStatistics;
}
std::uint64_t ClientModels::sessionRevision() const
{
    return m_sessionRevision;
}
std::uint64_t ClientModels::viewRevision() const
{
    return m_viewRevision;
}
bool ClientModels::messageHasMore() const
{
    const auto found = m_history.find(m_selectedChat);
    return found != m_history.end() && found->second.hasMore;
}
QString ClientModels::connectionState() const
{
    using State = core::ClientConnectionState;
    switch (m_state)
    {
    case State::Disconnected:
        return "Disconnected";
    case State::Connecting:
        return "Connecting";
    case State::Authenticating:
        return "Authenticating";
    case State::Synchronizing:
        return "Synchronizing";
    case State::Ready:
        return "Ready";
    case State::WaitingToReconnect:
        return "WaitingToReconnect";
    case State::Exhausted:
        return "Exhausted";
    }
    return {};
}
QString ClientModels::statusText() const
{
    using State = core::ClientConnectionState;
    switch (m_state)
    {
    case State::Disconnected:
        return "Нет связи";
    case State::Connecting:
        return QString{"Подключение: попытка %1 из 5"}.arg(m_attempt);
    case State::Authenticating:
        return "Авторизация";
    case State::Synchronizing:
        return "Загрузка данных";
    case State::Ready:
        return online() ? "Подключено" : "Проверка данных";
    case State::WaitingToReconnect:
        return QString{"Нет связи. Попытка %1 через %2 с"}.arg(m_attempt).arg(
            std::chrono::ceil<std::chrono::seconds>(std::chrono::milliseconds{m_retryDelayMs}).count());
    case State::Exhausted:
        return "Нет связи. Нажмите «Переподключиться»";
    }
    return {};
}
void ClientModels::setConnectionState(core::ClientConnectionState state, int attempt, int retryDelayMs)
{
    m_state = state;
    m_attempt = attempt;
    m_retryDelayMs = retryDelayMs;
    if (state != core::ClientConnectionState::Ready)
    {
        m_hasSnapshot = false;
        for (auto* model : {&m_tasks, &m_reminders, &m_habits, &m_eventsToday, &m_calendarEvents, &m_chats, &m_messages,
                            &m_profile, &m_moods, &m_routes})
        {
            model->setStale(true);
        }
    }
    emit connectionChanged();
}
void ClientModels::invalidateSession()
{
    ++m_sessionRevision;
    m_hasSnapshot = false;
    for (auto* model : {&m_tasks, &m_reminders, &m_habits, &m_eventsToday, &m_calendarEvents, &m_chats, &m_messages,
                        &m_profile, &m_moods, &m_routes})
    {
        model->setStale(true);
    }
    emit connectionChanged();
}
void ClientModels::resetServerData()
{
    invalidateSession();
    ++m_viewRevision;
    m_history.clear();
    m_selectedChat = 0;
    m_mainChat = 0;
    m_timeZone.clear();
    m_serverTime = {};
    m_day.reset();
    m_month.reset();
    m_monthDays.clear();
    m_routeStatistics.clear();
    for (auto* model : {&m_tasks, &m_reminders, &m_habits, &m_eventsToday, &m_calendarEvents, &m_chats, &m_messages,
                        &m_profile, &m_moods, &m_routes})
    {
        static_cast<void>(model->replaceRows({}, true));
    }
    clearError();
    setConnectionState(core::ClientConnectionState::Disconnected);
    emit metadataChanged();
    emit selectionChanged();
    emit paginationChanged();
    emit calendarChanged();
    emit routeStatisticsChanged();
}
bool ClientModels::applyResult(const core::ClientReadResult& result, std::uint64_t session, std::uint64_t view)
{
    if (session != m_sessionRevision || m_state != core::ClientConnectionState::Ready)
    {
        return false;
    }
    return std::visit(
        [this, view](const auto& value) -> bool
        {
            using ResultType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<ResultType, core::ClientSnapshot>)
            {
                const auto tasks = rows(value.tasks);
                const auto reminders = rows(value.reminders);
                const auto habits = rows(value.habits);
                const auto events = rows(value.eventsToday);
                if (value.mainChatIdentifier <= 0 || !m_tasks.accepts(tasks) || !m_reminders.accepts(reminders) ||
                    !m_habits.accepts(habits) || !m_eventsToday.accepts(events))
                {
                    return false;
                }
                static_cast<void>(m_tasks.replaceRows(tasks));
                static_cast<void>(m_reminders.replaceRows(reminders));
                static_cast<void>(m_habits.replaceRows(habits));
                static_cast<void>(m_eventsToday.replaceRows(events));
                m_mainChat = value.mainChatIdentifier;
                m_serverTime = value.serverTime;
                m_timeZone = value.timeZone;
                m_hasSnapshot = true;
                emit metadataChanged();
                emit connectionChanged();
                return true;
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientChatList>)
            {
                if (!m_chats.replaceRows(rows(value.chats)))
                {
                    return false;
                }
                m_mainChat = value.mainChatIdentifier;
                emit metadataChanged();
                return true;
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientMessagePage>)
            {
                return applyPage(value, view);
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientCalendarDay>)
            {
                if (!m_day || value.date != *m_day)
                {
                    return false;
                }
                return m_calendarEvents.replaceRows(rows(value.events));
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientCalendarMonth>)
            {
                if (!m_month || value.month != *m_month)
                {
                    return false;
                }
                m_monthDays.clear();
                for (const auto day : value.days)
                {
                    m_monthDays.append(static_cast<int>(day));
                }
                emit calendarChanged();
                return true;
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientProfile>)
            {
                return m_profile.replaceRows(rows(value.facts));
            }
            else if constexpr (std::is_same_v<ResultType, core::ClientMoods>)
            {
                return m_moods.replaceRows(rows(value.entries));
            }
            else
            {
                if (!m_routes.replaceRows(rows(value.recent)))
                {
                    return false;
                }
                m_routeStatistics = {
                    {"routedCount", QVariant::fromValue<qulonglong>(value.statistics.routedCount)},
                    {"approximateCount", QVariant::fromValue<qulonglong>(value.statistics.approximateCount)},
                    {"plannedDistanceMeters", value.statistics.plannedDistanceMeters},
                    {"plannedDurationSeconds", value.statistics.plannedDurationSeconds}};
                emit routeStatisticsChanged();
                return true;
            }
        },
        result);
}
bool ClientModels::selectChat(const QString& identifier)
{
    const auto parsed = parsedIdentifier(identifier);
    if (!parsed || !m_chats.containsIdentifier(identifier))
    {
        return false;
    }
    if (m_selectedChat == *parsed)
    {
        return true;
    }
    m_selectedChat = *parsed;
    ++m_viewRevision;
    m_history.try_emplace(m_selectedChat);
    displayMessages();
    emit selectionChanged();
    emit paginationChanged();
    if (online())
    {
        refreshMessages(true);
    }
    return true;
}
void ClientModels::displayMessages()
{
    const auto found = m_history.find(m_selectedChat);
    if (found == m_history.end())
    {
        static_cast<void>(m_messages.replaceRows({}, true));
        return;
    }
    static_cast<void>(
        m_messages.replaceRows(rows(found->second.messages), !online() || found->second.session != m_sessionRevision));
}
bool ClientModels::applyPage(const core::ClientMessagePage& page, std::uint64_t view)
{
    if (view != m_viewRevision || page.chatIdentifier != m_selectedChat)
    {
        return false;
    }
    auto& cache = m_history[m_selectedChat];
    if (page.afterMessageIdentifier &&
        (cache.session != m_sessionRevision || cache.cursor != page.afterMessageIdentifier))
    {
        return false;
    }
    auto messages = page.afterMessageIdentifier ? cache.messages : std::vector<core::ChatMessage>{};
    auto previous = page.afterMessageIdentifier.value_or(0);
    for (const auto& item : page.messages)
    {
        if (item.chatIdentifier != m_selectedChat || item.identifier <= previous)
        {
            return false;
        }
        messages.push_back(item);
        previous = item.identifier;
    }
    if (page.nextAfterMessageIdentifier != (previous == 0 ? std::nullopt : std::optional{previous}) ||
        (page.hasMore && page.messages.empty()) || !m_messages.accepts(rows(messages)))
    {
        return false;
    }
    cache = ChatCache{std::move(messages), page.nextAfterMessageIdentifier, page.hasMore, m_sessionRevision};
    displayMessages();
    emit paginationChanged();
    return true;
}
void ClientModels::requestRead(core::ClientReadRequest request)
{
    if (online())
    {
        using Kind = core::ClientReadKind;
        switch (request.kind)
        {
        case Kind::Snapshot:
            for (auto* model : {&m_tasks, &m_reminders, &m_habits, &m_eventsToday})
            {
                model->setStale(true);
            }
            break;
        case Kind::Chats:
            m_chats.setStale(true);
            break;
        case Kind::Messages:
            m_messages.setStale(true);
            break;
        case Kind::CalendarDay:
            m_calendarEvents.setStale(true);
            break;
        case Kind::Profile:
            m_profile.setStale(true);
            break;
        case Kind::Moods:
            m_moods.setStale(true);
            break;
        case Kind::Routes:
            m_routes.setStale(true);
            break;
        case Kind::CalendarMonth:
            break;
        }
        emit readRequested(request, m_sessionRevision, m_viewRevision);
    }
}
void ClientModels::refreshMessages(bool restart)
{
    if (m_selectedChat <= 0 || !online())
    {
        return;
    }
    const auto& cache = m_history[m_selectedChat];
    requestRead(
        {.kind = core::ClientReadKind::Messages,
         .chatIdentifier = m_selectedChat,
         .afterMessageIdentifier = restart || cache.session != m_sessionRevision ? std::nullopt : cache.cursor});
}
bool ClientModels::loadMoreMessages()
{
    if (!online() || !messageHasMore())
    {
        return false;
    }
    refreshMessages(false);
    return true;
}
void ClientModels::refresh()
{
    if (!online())
    {
        return;
    }
    for (const auto kind : {core::ClientReadKind::Snapshot, core::ClientReadKind::Chats, core::ClientReadKind::Profile,
                            core::ClientReadKind::Moods, core::ClientReadKind::Routes})
    {
        requestRead({.kind = kind});
    }
    refreshMessages(false);
    if (m_day)
    {
        requestRead({.kind = core::ClientReadKind::CalendarDay, .date = *m_day});
    }
    if (m_month)
    {
        requestRead({.kind = core::ClientReadKind::CalendarMonth, .month = *m_month});
    }
}
bool ClientModels::showDay(const QString& text)
{
    const auto parsed = QDate::fromString(text, Qt::ISODate);
    if (!parsed.isValid() || parsed.year() < 1 || parsed.year() > 9999 || parsed.toString(Qt::ISODate) != text)
    {
        return false;
    }
    m_day =
        std::chrono::year{parsed.year()} / static_cast<unsigned>(parsed.month()) / static_cast<unsigned>(parsed.day());
    m_calendarEvents.setStale(true);
    requestRead({.kind = core::ClientReadKind::CalendarDay, .date = *m_day});
    return true;
}
bool ClientModels::showMonth(int year, int month)
{
    if (year < 1 || year > 9999 || month < 1 || month > 12)
    {
        return false;
    }
    m_month = std::chrono::year{year} / static_cast<unsigned>(month);
    requestRead({.kind = core::ClientReadKind::CalendarMonth, .month = *m_month});
    return true;
}
bool ClientModels::submit(core::ClientAction action)
{
    if (!online())
    {
        setError("Нет связи: действие не отправлено.");
        return false;
    }
    emit actionRequested(std::move(action));
    return true;
}
bool ClientModels::setTaskDone(const QString& identifier, bool done)
{
    const auto parsed = parsedIdentifier(identifier);
    if (!parsed || !m_tasks.containsIdentifier(identifier))
    {
        setError("Задача не найдена.");
        return false;
    }
    return submit(core::ClientSetTaskDone{*parsed, done});
}
bool ClientModels::sendMessage(const QString& text, const QString& query)
{
    if (m_selectedChat <= 0 || text.trimmed().isEmpty())
    {
        setError("Выберите беседу и введите сообщение.");
        return false;
    }
    return submit(
        core::ClientSendMessage{m_selectedChat, text.toStdString(),
                                query.trimmed().isEmpty() ? std::nullopt : std::optional{query.toStdString()}});
}
bool ClientModels::retryMessage(const QString& userIdentifier, const QString& query)
{
    const auto parsed = parsedIdentifier(userIdentifier);
    if (!parsed || m_selectedChat <= 0)
    {
        setError("Укажите исходное сообщение выбранной беседы.");
        return false;
    }
    return submit(core::ClientRetryMessage{
        m_selectedChat, *parsed, query.trimmed().isEmpty() ? std::nullopt : std::optional{query.toStdString()}});
}
bool ClientModels::createChat()
{
    return submit(core::ClientCreateChat{});
}
void ClientModels::reconnect()
{
    emit reconnectRequested();
}
bool ClientModels::submitPlannerAction(core::ClientPlannerAction action)
{
    return submit(std::move(action));
}
void ClientModels::finishPlannerAction(const core::ClientPlannerAction& action, bool success, const QString& message,
                                       const QString& effect)
{
    emit plannerActionFinished(action, success, success ? QString{} : (message.isNull() ? errorText() : message),
                               success ? QString{"none"} : (effect.isNull() ? errorEffect() : effect));
}
void ClientModels::setError(const QString& message, const QString& effect, const QString& savedUser)
{
    if (!message.isEmpty() && effect == "none" && (m_effect == "unknown" || m_effect == "user_saved"))
    {
        return;
    }
    m_error = message;
    m_effect = effect;
    m_savedUser = savedUser;
    emit errorChanged();
}
void ClientModels::clearError()
{
    setError({});
}
} // namespace msa::ui
