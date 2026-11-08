#include "ServerRuntime.h"

#include "msa/infra/ProtocolPayload.h"
#include "msa/infra/SqliteSchema.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QScopedValueRollback>
#include <QStringList>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace msa::server {
namespace {
using Payload = infra::ProtocolPayload;

struct Rejection {
    QString code;
    QString message;
};

[[noreturn]] void reject(const QString& code, const QString& message)
{
    throw Rejection{code, message};
}

void require(bool exists)
{
    if (!exists) { reject("not_found", "Запись не найдена."); }
}

template <typename ValueType, typename RejectionType>
ValueType accepted(const std::variant<ValueType, RejectionType>& result)
{
    if (const auto* rejection = std::get_if<RejectionType>(&result)) {
        reject("validation_failed", QString::fromStdString(rejection->reason));
    }

    return std::get<ValueType>(result);
}

template <typename ValueType> ValueType externalResult(const core::ExternalServiceResult<ValueType>& result,
    bool searching = false)
{
    if (const auto* error = std::get_if<core::ExternalServiceError>(&result)) {
        QString code{"validation_failed"};
        switch (error->kind) {
        case core::ExternalServiceErrorKind::InvalidRequest: break;
        case core::ExternalServiceErrorKind::NotFound: code = "not_found"; break;
        case core::ExternalServiceErrorKind::Unavailable: code = searching ? "search_unavailable" : "conflict"; break;
        case core::ExternalServiceErrorKind::InvalidResponse:
            code = searching ? "search_invalid_response" : "conflict"; break;
        }
        reject(code, QString::fromStdString(error->reason));
    }

    return std::get<ValueType>(result);
}

std::int64_t identifier(const QJsonObject& params, const char* name)
{
    return params.value(QLatin1String{name}).toString().toLongLong();
}

std::string text(const QJsonObject& params, const char* name)
{
    return params.value(QLatin1String{name}).toString().toStdString();
}

std::chrono::year_month_day date(const QJsonValue& value)
{
    const auto parsed = QDate::fromString(value.toString(), Qt::ISODate);

    return std::chrono::year{parsed.year()} / static_cast<unsigned>(parsed.month()) / static_cast<unsigned>(parsed.day());
}

std::optional<std::chrono::year_month_day> optionalDate(const QJsonValue& value)
{
    if (value.isNull()) { return std::nullopt; }

    return date(value);
}

std::optional<std::chrono::minutes> optionalTime(const QJsonValue& value)
{
    if (value.isNull()) { return std::nullopt; }
    const auto parsed = QTime::fromString(value.toString(), "HH:mm");

    return std::chrono::hours{parsed.hour()} + std::chrono::minutes{parsed.minute()};
}

std::chrono::local_seconds localTime(const QJsonValue& value)
{
    const auto parsed = QDateTime::fromString(value.toString() + "Z", Qt::ISODate);

    return std::chrono::local_seconds{std::chrono::seconds{parsed.toSecsSinceEpoch()}};
}

std::chrono::year_month_day today(const core::Clock& clock)
{
    return std::chrono::year_month_day{std::chrono::sys_days{
        std::chrono::floor<std::chrono::days>(clock.now()).time_since_epoch()}};
}

core::GeoCoordinates coordinates(const QJsonValue& value)
{
    const auto object = value.toObject();

    return {object.value("latitude").toDouble(), object.value("longitude").toDouble()};
}

core::TaskPriority priority(const QJsonObject& params)
{
    const auto value = params.value("priority").toString();

    return value == "High" ? core::TaskPriority::High : value == "Low" ? core::TaskPriority::Low : core::TaskPriority::Normal;
}

core::EventRepeat repeat(const QJsonObject& params)
{
    const auto value = params.value("repeat").toString();
    if (value == "Daily") { return core::EventRepeat::Daily; }
    if (value == "Weekly") { return core::EventRepeat::Weekly; }
    if (value == "Monthly") { return core::EventRepeat::Monthly; }

    return core::EventRepeat::None;
}

core::MoodScore score(const QJsonObject& params)
{
    const auto value = params.value("score").toString();

    return value == "Positive" ? core::MoodScore::Positive
        : value == "Negative" ? core::MoodScore::Negative : core::MoodScore::Neutral;
}

core::TravelMode mode(const QJsonObject& params)
{
    const auto value = params.value("mode").toString();

    return value == "Driving" ? core::TravelMode::Driving
        : value == "Bike" ? core::TravelMode::Bike : core::TravelMode::Foot;
}

template <typename Range> QJsonArray objects(const Range& range)
{
    QJsonArray result;
    for (const auto& item : range) { result.append(Payload::object(item)); }

    return result;
}

QJsonObject saved(const char* name, std::int64_t value)
{
    return {{QLatin1String{name}, Payload::identifier(value)}};
}

bool readOnly(const QString& method)
{
    return method == "ping" || method == "get_all" || method == "chat.list" || method == "chat.messages"
        || method == "profile.get" || method == "mood.history" || method == "route.history"
        || method == "calendar.day" || method == "calendar.month";
}

core::ConversationService conversationService(const core::Clock& clock, core::ChatService& chat,
    core::UserProfileService& profile, const core::DailyContextService& daily, ExternalServices external,
    core::MoodService& moods, core::EveningCheckInService& checkIn,
    infra::SqliteEveningCheckInRepository& checkIns, infra::SqliteDatabase& database)
{
    return {clock, chat, profile, daily, external.llama, moods, checkIn, checkIns, database, external.search};
}
} // namespace

ServerRuntime::ServerRuntime(const ServerSettings& settings, const core::Clock& clock, ExternalServices external)
    : m_settings{settings}, m_clock{clock}, m_external{external},
      m_database{settings.databasePath, infra::sqliteMigrations()}, m_chats{m_database}, m_tasks{m_database},
      m_events{m_database}, m_reminders{m_database}, m_habits{m_database}, m_moods{m_database},
      m_profile{m_database}, m_greetings{m_database}, m_checkIns{m_database}, m_warnings{m_database},
      m_cache{m_database}, m_routes{m_database}, m_taskService{clock, m_tasks}, m_eventService{m_events},
      m_reminderService{clock, m_reminders}, m_habitService{clock, m_habits}, m_moodService{clock, m_moods},
      m_profileService{m_profile}, m_calendar{m_events}, m_daily{clock, m_calendar, m_taskService, m_habitService},
      m_greetingService{clock, m_calendar, m_tasks, m_greetings}, m_checkInService{clock, m_checkIns},
      m_warningService{clock, m_calendar, m_warnings},
      m_geocoder{external.geocoder, m_cache, clock, external.geocodingSourceKey},
      m_routeHistory{m_routes, clock}, m_navigation{m_geocoder, external.planner, m_routeHistory},
      m_locations{m_reminders, m_geocoder}, m_mainChat{m_chats.ensureMainChat(clock.now())},
      m_transport{settings.token, settings.timeZone, clock}
{
    connect(&m_transport, &infra::TcpServer::requestReceived, this,
        [this](quint64 connectionId, const infra::ProtocolRequest& request) { receive(connectionId, request); });
    connect(&m_transport, &infra::TcpServer::connectionClosed, this,
        [this](quint64 connectionId) { closeSession(connectionId); });
    connect(&m_timer, &QTimer::timeout, this, [this]() { checkDeadlines(); });
}

ServerRuntime::~ServerRuntime()
{
    m_timer.stop();
    m_transport.close();
    QObject::disconnect(&m_transport, nullptr, this, nullptr);
}

bool ServerRuntime::start()
{
    if (m_timer.isActive()) { return false; }
    m_database.transaction([this]() { m_reminderService.skipMissedOnStartup(); });
    if (!m_transport.listen(m_settings.listenAddress, m_settings.port)) { return false; }
    m_timer.start(m_settings.checkIntervalMs);
    checkDeadlines();

    return true;
}

quint16 ServerRuntime::port() const
{
    return m_transport.serverPort();
}

void ServerRuntime::receive(quint64 connectionId, const infra::ProtocolRequest& request)
{
    // Слот транспорта возвращается сразу; тяжёлая связка запускается отдельным событием.
    QTimer::singleShot(0, this, [this, connectionId, request]() { dispatch(connectionId, request); });
}

void ServerRuntime::dispatch(quint64 connectionId, const infra::ProtocolRequest& request)
{
    if (m_closedSessions.contains(connectionId)) { return; }
    std::optional<QString> savedUser;
    try {
        if (m_busy && !readOnly(request.method)) { reject("busy", "Сервер выполняет другую операцию. Повторите позже."); }
        QJsonObject result;
        if (readOnly(request.method)) {
            result = read(request);
        } else {
            const QScopedValueRollback<bool> busy{m_busy, true};
            result = execute(connectionId, request, savedUser);
        }
        static_cast<void>(m_transport.sendResponse(connectionId, infra::ProtocolSuccess{request.requestId, result}));
    } catch (const Rejection& rejection) {
        static_cast<void>(m_transport.sendResponse(connectionId, infra::ProtocolFailure{request.requestId,
            rejection.code, rejection.message, savedUser ? "user_saved" : "none", savedUser}));
    } catch (...) {
        // SQL и внутренние адреса никогда не попадают в ответ или журнал.
        static_cast<void>(m_transport.sendResponse(connectionId, infra::ProtocolFailure{request.requestId,
            "storage_error", "Не удалось завершить операцию. Перечитайте данные перед повтором.", "unknown", std::nullopt}));
    }
}

QJsonObject ServerRuntime::read(const infra::ProtocolRequest& request)
{
    const auto& params = request.params;
    const auto& method = request.method;
    if (method == "ping") { return {{"server_time", Payload::localDateTime(m_clock.now())}}; }
    if (method == "get_all") {
        QJsonArray events;
        const auto currentDate = today(m_clock);
        for (const auto& event : m_calendar.eventsForDate(currentDate)) {
            events.append(Payload::occurrence(event, currentDate));
        }
        return {{"server_time", Payload::localDateTime(m_clock.now())}, {"time_zone", m_settings.timeZone},
            {"main_chat_id", Payload::identifier(m_mainChat)}, {"tasks", objects(m_taskService.tasks())},
            {"reminders", objects(m_reminderService.activeReminders())}, {"habits", objects(m_habitService.habits())},
            {"events_today", events}};
    }
    if (method == "chat.list") {
        const core::ChatService chat{m_clock, m_chats, m_mainChat};
        return {{"chats", objects(chat.chatList())}, {"main_chat_id", Payload::identifier(m_mainChat)}};
    }
    if (method == "chat.messages") {
        const auto chatId = identifier(params, "chat_id");
        require(m_chats.findChat(chatId).has_value());
        const core::ChatService chat{m_clock, m_chats, chatId};
        const auto history = chat.loadMessages();
        const auto after = params.value("after_message_id").isNull() ? 0 : identifier(params, "after_message_id");
        QJsonArray page;
        std::int64_t last = after;
        bool more{false};
        QJsonObject result{{"chat_id", Payload::identifier(chatId)}, {"messages", page},
            {"next_after_message_id", after == 0 ? QJsonValue{QJsonValue::Null} : QJsonValue{Payload::identifier(after)}},
            {"has_more", false}};
        for (const auto& message : history) {
            if (message.identifier <= after) { continue; }
            if (page.size() >= params.value("limit").toInt()) { more = true; break; }
            auto candidate = page;
            candidate.append(Payload::object(message));
            result["messages"] = candidate;
            result["next_after_message_id"] = Payload::identifier(message.identifier);
            // Проверяем полный конверт с исходным request_id, сохраняя запас для has_more=false.
            result["has_more"] = false;
            if (std::holds_alternative<infra::ProtocolDecodeError>(
                infra::ProtocolCodec::encode(infra::ProtocolSuccess{request.requestId, result}))) {
                if (page.isEmpty()) { reject("response_too_large", "Сообщение не помещается в кадр протокола."); }
                more = true;
                break;
            }
            page = candidate;
            last = message.identifier;
        }
        result["messages"] = page;
        result["next_after_message_id"] = last == 0 ? QJsonValue{QJsonValue::Null} : QJsonValue{Payload::identifier(last)};
        result["has_more"] = more;

        return result;
    }
    if (method == "calendar.day") {
        const auto requestedDate = date(params.value("date"));
        QJsonArray events;
        for (const auto& event : m_calendar.eventsForDate(requestedDate)) {
            events.append(Payload::occurrence(event, requestedDate));
        }
        return {{"date", params.value("date")}, {"events", events}};
    }
    if (method == "calendar.month") {
        QJsonArray days;
        const auto month = std::chrono::year{params.value("year").toInt()}
            / static_cast<unsigned>(params.value("month").toInt());
        for (const auto day : m_calendar.eventDaysInMonth(month)) { days.append(static_cast<int>(static_cast<unsigned>(day))); }

        return {{"year", params.value("year")}, {"month", params.value("month")}, {"days", days}};
    }
    if (method == "profile.get") { return {{"facts", objects(m_profileService.profile().facts)}}; }
    if (method == "mood.history") { return {{"entries", objects(accepted(m_moodService.moodHistory(params.value("days").toInt())))}}; }
    if (method == "route.history") {
        return {{"recent", objects(m_routeHistory.recent())}, {"statistics", Payload::object(m_routeHistory.statistics())}};
    }
    reject("unknown_method", "Неизвестная операция.");
}

QJsonObject ServerRuntime::execute(quint64 connectionId, const infra::ProtocolRequest& request,
    std::optional<QString>& savedUser)
{
    const auto& params = request.params;
    const auto& method = request.method;
    if (method == "chat.send" || method == "chat.retry") { return conversation(request, savedUser); }
    if (method == "command.execute") { return command(request); }
    if (method == "route.build") { return route(params); }
    if (method == "weather.get") {
        const auto forecast = externalResult(m_external.weather.forecast(coordinates(params.value("coordinates"))));

        auto result = Payload::object(forecast);
        result["text"] = QString::fromStdString(core::weatherText(forecast));
        return result;
    }
    if (method == "reminder.location.prepare") { return prepareLocation(connectionId, params); }
    if (method == "reminder.location.cancel" || method == "reminder.location.confirm") {
        const auto proposalId = identifier(params, "proposal_id");
        const auto found = m_proposals.find(proposalId);
        require(found != m_proposals.end() && found->second.first == connectionId);
        if (method == "reminder.location.cancel") {
            require(m_locations.cancel(proposalId));
            m_proposals.erase(found);
            return {{"cancelled", true}};
        }
        const auto reminderId = found->second.second;
        const auto result = m_locations.confirm(proposalId, static_cast<std::size_t>(params.value("candidate_index").toInt()));
        if (const auto* error = std::get_if<core::ExternalServiceError>(&result)) {
            reject("stale_proposal", QString::fromStdString(error->reason));
        }
        m_proposals.erase(found);
        changed({"reminders"});
        return {{"reminder_id", Payload::identifier(reminderId)},
            {"coordinates", Payload::object(std::get<core::GeoCoordinates>(result))}};
    }
    if (method == "chat.create") {
        core::ChatService chat{m_clock, m_chats, m_mainChat};
        const auto chatId = chat.newChat();
        changed({"chats"});
        return saved("chat_id", chatId);
    }
    if (method == "task.create") {
        const auto taskId = accepted(m_taskService.addTask(text(params, "title"), optionalDate(params.value("due_date")), priority(params)));
        changed({"tasks"});
        return saved("task_id", taskId);
    }
    if (method == "task.update" || method == "task.set_done") {
        const auto taskId = identifier(params, "task_id");
        require(m_tasks.find(taskId).has_value());
        if (method == "task.update") {
            static_cast<void>(accepted(m_taskService.updateTask(taskId, text(params, "title"),
                optionalDate(params.value("due_date")), priority(params))));
        } else { static_cast<void>(accepted(m_taskService.setTaskDone(taskId, params.value("done").toBool()))); }
        changed({"tasks"});
        return saved("task_id", taskId);
    }
    if (method == "event.create" || method == "event.update") {
        std::int64_t eventId{};
        if (method == "event.create") {
            eventId = accepted(m_eventService.addEvent(text(params, "title"), date(params.value("date")),
                optionalTime(params.value("time")), text(params, "description"), repeat(params)));
        } else {
            eventId = identifier(params, "event_id");
            require(m_events.find(eventId).has_value());
            static_cast<void>(accepted(m_eventService.updateEvent(eventId, text(params, "title"), date(params.value("date")),
                optionalTime(params.value("time")), text(params, "description"), repeat(params))));
        }
        changed({"events"});
        return saved("event_id", eventId);
    }
    if (method == "habit.create") {
        const auto habitId = accepted(m_habitService.addHabit(text(params, "name"), params.value("target").toDouble(),
            text(params, "unit"), params.value("period").toString() == "Week" ? core::HabitPeriod::Week : core::HabitPeriod::Day));
        changed({"habits"});
        return saved("habit_id", habitId);
    }
    if (method == "habit.log") {
        require(m_habits.find(identifier(params, "habit_id")).has_value());
        const auto logId = accepted(m_habitService.logHabit(identifier(params, "habit_id"), params.value("value").toDouble()));
        changed({"habits"});
        return saved("log_id", logId);
    }
    if (method == "reminder.create") {
        const auto kind = params.value("kind").toString();
        std::int64_t reminderId{};
        if (kind == "Timed") { reminderId = accepted(m_reminderService.addTimed(text(params, "title"), localTime(params.value("due_at")))); }
        else if (kind == "Recurring") {
            reminderId = accepted(m_reminderService.addRecurring(text(params, "title"),
                std::chrono::minutes{params.value("interval_minutes").toInt()}));
        } else { reminderId = accepted(m_reminderService.addPlace(text(params, "title"), text(params, "place"))); }
        changed({"reminders"});
        return saved("reminder_id", reminderId);
    }
    if (method == "profile.remember") {
        std::vector<std::string> tags;
        for (const auto& tag : params.value("tags").toArray()) { tags.push_back(tag.toString().toStdString()); }
        static_cast<void>(accepted(m_profileService.rememberFact(text(params, "text"), tags, params.value("always_include").toBool())));
        const auto factId = m_profileService.profile().facts.back().identifier;
        changed({"profile"});
        return saved("fact_id", factId);
    }
    if (method == "mood.record") {
        const auto moodId = accepted(m_moodService.logMood(score(params), text(params, "note")));
        changed({"moods"});
        return saved("mood_id", moodId);
    }
    if (method == "location.update") {
        std::vector<core::PlaceReminder> due;
        m_database.transaction([this, &params, &due]() {
            due = accepted(m_reminderService.checkNearPlace(coordinates(params.value("coordinates"))));
        });
        for (const auto& reminder : due) { reminderDue(reminder); }
        if (!due.empty()) { changed({"reminders"}); }
        return {{"checked", true}};
    }
    QString scope;
    bool removed{false};
    if (method == "task.delete") { removed = m_tasks.remove(identifier(params, "task_id")); scope = "tasks"; }
    else if (method == "event.delete") { removed = m_events.remove(identifier(params, "event_id")); scope = "events"; }
    else if (method == "habit.delete") { removed = m_habits.remove(identifier(params, "habit_id")); scope = "habits"; }
    else if (method == "reminder.delete") { removed = m_reminders.remove(identifier(params, "reminder_id")); scope = "reminders"; }
    else { reject("unknown_method", "Неизвестная операция."); }
    require(removed);
    changed({scope});

    return {{"deleted", true}};
}

QJsonObject ServerRuntime::conversation(const infra::ProtocolRequest& request, std::optional<QString>& savedUser)
{
    const auto& params = request.params;
    const auto chatId = identifier(params, "chat_id");
    require(m_chats.findChat(chatId).has_value());
    core::ChatService chat{m_clock, m_chats, chatId};
    auto service = conversationService(m_clock, chat, m_profileService, m_daily, m_external,
        m_moodService, m_checkInService, m_checkIns, m_database);
    std::int64_t userId{};
    if (request.method == "chat.send") {
        userId = accepted(chat.addMessage(text(params, "text"), core::MessageRole::User));
        savedUser = Payload::identifier(userId);
        chatChanged(chatId);
    } else {
        const auto history = chat.loadMessages();
        if (history.empty() || history.back().role != core::MessageRole::User
            || history.back().identifier != identifier(params, "expected_user_message_id")) {
            reject("conflict", "Последнее сообщение изменилось. Перечитайте историю.");
        }
        userId = history.back().identifier;
        savedUser = Payload::identifier(userId);
    }
    const auto result = params.value("search_query").isNull()
        ? service.retryLastMessage(m_settings.contextMaxChars)
        : service.retryLastMessageWithSearch(text(params, "search_query"), m_settings.contextMaxChars);
    if (const auto* failure = std::get_if<core::LlmFailure>(&result)) {
        reject(failure->kind == core::LlmFailureKind::Unavailable ? "llm_unavailable" : "llm_invalid_response",
            QString::fromStdString(failure->reason));
    }
    if (const auto* failure = std::get_if<core::ExternalServiceError>(&result)) {
        static_cast<void>(externalResult(core::ExternalServiceResult<int>{*failure}, true));
    }
    if (const auto* failure = std::get_if<core::ConversationRejection>(&result)) {
        const auto code = failure->kind == core::ConversationRejectionKind::ContextBudgetExceeded
            ? "context_budget_exceeded" : failure->kind == core::ConversationRejectionKind::Conflict
            ? "conflict" : "validation_failed";
        reject(code, QString::fromStdString(failure->reason));
    }
    const auto& reply = std::get<core::ConversationReply>(result);
    chatChanged(chatId);
    QJsonArray facts;
    for (const auto factId : reply.profileFactIdentifiers) { facts.append(Payload::identifier(factId)); }
    QJsonValue tone{QJsonValue::Null};
    if (reply.tone) {
        tone = *reply.tone == core::MoodScore::Positive ? "Positive"
            : *reply.tone == core::MoodScore::Negative ? "Negative" : "Neutral";
    }
    QJsonArray scopes{"messages"};
    if (reply.moodIdentifier) { scopes.append("moods"); }
    if (!reply.profileFactIdentifiers.empty()) { scopes.append("profile"); }
    changed(scopes);

    return {{"chat_id", Payload::identifier(chatId)}, {"user_message_id", Payload::identifier(userId)},
        {"message_id", Payload::identifier(reply.messageIdentifier)}, {"text", QString::fromStdString(reply.text)},
        {"tone", tone}, {"mood_id", reply.moodIdentifier ? QJsonValue{Payload::identifier(*reply.moodIdentifier)} : QJsonValue{QJsonValue::Null}},
        {"profile_fact_ids", facts}, {"search_sources", objects(reply.searchSources)}};
}

QJsonObject ServerRuntime::route(const QJsonObject& params)
{
    const auto travelMode = mode(params);
    const auto destination = text(params, "destination");
    const auto origin = params.value("origin");
    const auto result = origin.isObject()
        ? m_navigation.routeFrom(coordinates(origin), destination, travelMode)
        : m_navigation.routeBetween(origin.toString().toStdString(), destination,
            coordinates(params.value("reference")), travelMode);
    const auto plan = externalResult(result);
    changed({"routes"});

    return {{"calculation_id", Payload::identifier(*plan.calculationIdentifier)},
        {"distance_meters", plan.distanceMeters}, {"duration_seconds", plan.durationSeconds},
        {"approximate", plan.approximate}, {"steps", objects(plan.steps)},
        {"text", QString::fromStdString(core::routeText(plan, travelMode,
            origin.isString() ? origin.toString().toStdString() : "Моё местоположение", destination))}};
}

QJsonObject ServerRuntime::prepareLocation(quint64 connectionId, const QJsonObject& params)
{
    const auto reminderId = identifier(params, "reminder_id");
    require(m_reminders.find(reminderId).has_value());
    const auto proposal = externalResult(m_locations.prepare(reminderId, coordinates(params.value("reference"))));
    if (m_closedSessions.contains(connectionId)) {
        static_cast<void>(m_locations.cancel(proposal.identifier));
        reject("stale_proposal", "Соединение закрыто.");
    }
    // Новое предложение сервиса отменяет предыдущее для того же напоминания.
    std::erase_if(m_proposals, [reminderId](const auto& entry) { return entry.second.second == reminderId; });
    m_proposals.emplace(proposal.identifier, std::pair{connectionId, reminderId});
    QJsonArray candidates;
    for (const auto& candidate : proposal.candidates) {
        candidates.append(QJsonObject{{"name", QString::fromStdString(candidate.name)},
            {"coordinates", Payload::object(candidate.coordinates)}});
    }

    return {{"proposal_id", Payload::identifier(proposal.identifier)}, {"reminder_id", Payload::identifier(reminderId)},
        {"place", QString::fromStdString(proposal.place)}, {"candidates", candidates}};
}

QJsonObject ServerRuntime::command(const infra::ProtocolRequest& request)
{
    const auto& params = request.params;
    const auto chatId = identifier(params, "chat_id");
    require(m_chats.findChat(chatId).has_value());
    const core::CommandParser parser;
    const auto parsed = parser.parse(text(params, "text"), m_clock.now());
    if (std::holds_alternative<core::NoCommand>(parsed)) {
        return {{"handled", false}, {"reply", ""}, {"created_ids", QJsonArray{}}};
    }
    std::optional<core::PreparedRoute> prepared;
    if (const auto* routeCommand = std::get_if<core::RouteCommand>(&parsed)) {
        if (params.value("reference").isNull()) { reject("location_required", "Укажите местоположение для маршрута."); }
        prepared = externalResult(m_navigation.prepareFrom(coordinates(params.value("reference")),
            routeCommand->place, routeCommand->mode));
    }
    // Список целиком проверяется до первой записи; парсер уже проверил длину каждого пункта.
    if (const auto* list = std::get_if<core::TaskListCommand>(&parsed)) {
        if (list->items.empty() || std::ranges::any_of(list->items, [](const auto& item) {
            return QString::fromStdString(item).trimmed().isEmpty();
        })) { reject("validation_failed", "Укажите непустые названия всех задач."); }
    }
    core::ChatService chat{m_clock, m_chats, chatId};
    QJsonArray created;
    QJsonArray scopes{"messages"};
    QString reply;
    m_database.transaction([this, &chat, &parsed, &params, &created, &scopes, &reply, &prepared]() {
        std::visit([this, &created, &scopes, &reply, &prepared](const auto& value) {
            using CommandType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<CommandType, core::TaskCommand>) {
                created.append(Payload::identifier(accepted(m_taskService.addTask(value.title, value.dueDate))));
                scopes.append("tasks"); reply = "Задача добавлена.";
            } else if constexpr (std::is_same_v<CommandType, core::TaskListCommand>) {
                for (const auto& title : value.items) {
                    created.append(Payload::identifier(accepted(m_taskService.addTask(title, std::nullopt))));
                }
                scopes.append("tasks"); reply = "Список задач добавлен.";
            } else if constexpr (std::is_same_v<CommandType, core::EventCommand>) {
                created.append(Payload::identifier(accepted(m_eventService.addEvent(value.title, value.date, value.time))));
                scopes.append("events"); reply = "Событие добавлено.";
            } else if constexpr (std::is_same_v<CommandType, core::TimedReminderCommand>) {
                created.append(Payload::identifier(accepted(m_reminderService.addTimed(value.title, value.dueAt))));
                scopes.append("reminders"); reply = "Напоминание добавлено.";
            } else if constexpr (std::is_same_v<CommandType, core::RecurringReminderCommand>) {
                created.append(Payload::identifier(accepted(m_reminderService.addRecurring(value.title, value.interval))));
                scopes.append("reminders"); reply = "Повторяющееся напоминание добавлено.";
            } else if constexpr (std::is_same_v<CommandType, core::PlaceReminderCommand>) {
                created.append(Payload::identifier(accepted(m_reminderService.addPlace(value.title, value.place))));
                scopes.append("reminders"); reply = "Напоминание добавлено. Выберите и подтвердите адрес места.";
            } else if constexpr (std::is_same_v<CommandType, core::RememberCommand>) {
                static_cast<void>(accepted(m_profileService.rememberFact(value.fact)));
                created.append(Payload::identifier(m_profileService.profile().facts.back().identifier));
                scopes.append("profile"); reply = "Сохранено в памяти.";
            } else if constexpr (std::is_same_v<CommandType, core::RouteCommand>) {
                created.append(Payload::identifier(m_routeHistory.record(prepared->calculation)));
                scopes.append("routes");
                reply = QString::fromStdString(core::routeText(prepared->plan, value.mode, "Моё местоположение", value.place));
            }
        }, parsed);
        static_cast<void>(accepted(chat.addMessage(text(params, "text"), core::MessageRole::User)));
        static_cast<void>(accepted(chat.addMessage(reply.toStdString(), core::MessageRole::Assistant)));
    });
    chatChanged(chatId);
    changed(scopes);

    return {{"handled", true}, {"reply", reply}, {"created_ids", created}};
}

void ServerRuntime::changed(const QJsonArray& scopes)
{
    static_cast<void>(m_transport.publishEvent("state.changed", {{"scopes", scopes}}));
}

void ServerRuntime::chatChanged(std::int64_t chatIdentifier)
{
    static_cast<void>(m_transport.publishEvent("chat.changed", {{"chat_id", Payload::identifier(chatIdentifier)}}));
}

void ServerRuntime::assistantMessage(std::int64_t messageIdentifier)
{
    chatChanged(m_mainChat);
    static_cast<void>(m_transport.publishEvent("assistant.message", {{"chat_id", Payload::identifier(m_mainChat)},
        {"message_id", Payload::identifier(messageIdentifier)}}));
}

void ServerRuntime::reminderDue(const core::Reminder& reminder)
{
    std::visit([this](const auto& value) {
        static_cast<void>(m_transport.publishEvent("reminder.due", {{"reminder_id", Payload::identifier(value.identifier)},
            {"title", QString::fromStdString(value.title)}}));
    }, reminder);
}

void ServerRuntime::checkDeadlines()
{
    if (m_checking) { return; }
    const QScopedValueRollback<bool> checking{m_checking, true};
    try {
        std::vector<core::Reminder> reminders;
        std::vector<core::EventWarning> warnings;
        m_database.transaction([this, &reminders, &warnings]() {
            reminders = m_reminderService.checkDue();
            warnings = m_warningService.checkDue();
        });
        for (const auto& reminder : reminders) { reminderDue(reminder); }
        if (!reminders.empty()) { changed({"reminders"}); }
        for (const auto& warning : warnings) {
            const auto kind = warning.kind == core::EventWarningKind::BeforeStart ? "BeforeStart"
                : warning.kind == core::EventWarningKind::DayBefore ? "DayBefore" : "OnDay";
            static_cast<void>(m_transport.publishEvent("event.warning", {{"event_id", Payload::identifier(warning.event.identifier)},
                {"occurrence_date", Payload::date(warning.occurrenceDate)}, {"kind", kind},
                {"text", QString::fromStdString(warning.event.title)}}));
        }
        // Проактивная публикация не вклинивается между User и ожидаемым Assistant.
        if (m_busy) { return; }
        const QScopedValueRollback<bool> busy{m_busy, true};
        core::ChatService chat{m_clock, m_chats, m_mainChat};
        std::optional<std::int64_t> greeting;
        m_database.transaction([this, &chat, &greeting]() {
            if (const auto message = m_greetingService.greetingIfNeeded()) {
                greeting = accepted(chat.addMessage(*message, core::MessageRole::Assistant));
            }
        });
        if (greeting) { assistantMessage(*greeting); }
        auto service = conversationService(m_clock, chat, m_profileService, m_daily, m_external,
            m_moodService, m_checkInService, m_checkIns, m_database);
        if (const auto message = service.offerEveningCheckIn()) { assistantMessage(*message); }
    } catch (...) {
        qWarning("Не удалось проверить сроки или сохранить проактивное сообщение.");
    }
}

void ServerRuntime::closeSession(quint64 connectionId)
{
    m_closedSessions.insert(connectionId);
    std::erase_if(m_proposals, [this, connectionId](const auto& entry) {
        if (entry.second.first != connectionId) { return false; }
        static_cast<void>(m_locations.cancel(entry.first));
        return true;
    });
}
} // namespace msa::server

