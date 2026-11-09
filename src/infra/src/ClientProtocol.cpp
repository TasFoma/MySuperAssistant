#include "msa/infra/ClientProtocol.h"

#include "msa/infra/ProtocolCodec.h"
#include "msa/infra/ProtocolPayload.h"

#include <QDate>
#include <QJsonArray>
#include <QTime>
#include <QTimeZone>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace msa::infra
{
namespace
{
using Payload = ProtocolPayload;

void require(bool condition)
{
    if (!condition)
    {
        throw std::invalid_argument{"Неверные типизированные данные протокола клиента."};
    }
}

QJsonObject object(const QJsonValue& value)
{
    require(value.isObject());
    return value.toObject();
}
std::string string(const QJsonValue& value)
{
    require(value.isString());
    return value.toString().toStdString();
}
bool boolean(const QJsonValue& value)
{
    require(value.isBool());
    return value.toBool();
}
double number(const QJsonValue& value)
{
    require(value.isDouble() && std::isfinite(value.toDouble()));
    return value.toDouble();
}
std::int64_t identifier(const QJsonValue& value)
{
    require(ProtocolCodec::isEntityId(value));
    return value.toString().toLongLong();
}

std::optional<std::int64_t> optionalId(const QJsonValue& value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }
    return identifier(value);
}

int integer(const QJsonValue& value, int minimum, int maximum)
{
    const auto result = number(value);
    require(result >= minimum && result <= maximum && result == std::floor(result));
    return static_cast<int>(result);
}

std::chrono::year_month_day date(const QJsonValue& value)
{
    const auto text = QString::fromStdString(string(value));
    const auto parsed = QDate::fromString(text, Qt::ISODate);
    require(parsed.isValid() && parsed.year() >= 1 && parsed.year() <= 9999 && parsed.toString(Qt::ISODate) == text);
    return std::chrono::year{parsed.year()} / static_cast<unsigned>(parsed.month()) /
           static_cast<unsigned>(parsed.day());
}

std::chrono::local_seconds moment(const QJsonValue& value)
{
    const auto text = QString::fromStdString(string(value));
    require(text.size() == 19 && text.at(10) == 'T');
    const auto day = date(text.first(10));
    const auto time = QTime::fromString(text.sliced(11), "HH:mm:ss");
    require(time.isValid() && time.toString("HH:mm:ss") == text.sliced(11));
    return std::chrono::local_days{std::chrono::sys_days{day}.time_since_epoch()} + std::chrono::hours{time.hour()} +
           std::chrono::minutes{time.minute()} + std::chrono::seconds{time.second()};
}

template <typename EnumType>
EnumType enumeration(const QJsonValue& value, std::initializer_list<const char*> names, int offset = 0)
{
    const auto text = QString::fromStdString(string(value));
    int position{offset};
    for (const auto* name : names)
    {
        if (text == QLatin1String{name})
        {
            return static_cast<EnumType>(position);
        }
        ++position;
    }
    throw std::invalid_argument{"Неизвестное перечисление клиентского протокола."};
}

template <typename ValueType> ValueType checked(const QJsonObject& original, ValueType value)
{
    require(Payload::object(value) == original);
    return value;
}

core::Task task(const QJsonObject& value)
{
    return checked(
        value, core::Task{.identifier = identifier(value["id"]),
                          .title = string(value["title"]),
                          .dueDate = value["due_date"].isNull() ? std::nullopt : std::optional{date(value["due_date"])},
                          .priority = enumeration<core::TaskPriority>(value["priority"], {"Low", "Normal", "High"}),
                          .done = boolean(value["done"]),
                          .createdAt = moment(value["created_at"])});
}

core::Event event(const QJsonObject& value)
{
    std::optional<std::chrono::minutes> time;
    if (!value["time"].isNull())
    {
        const auto text = QString::fromStdString(string(value["time"]));
        const auto parsed = QTime::fromString(text, "HH:mm");
        require(parsed.isValid() && parsed.toString("HH:mm") == text);
        time = std::chrono::hours{parsed.hour()} + std::chrono::minutes{parsed.minute()};
    }
    return checked(value, core::Event{.identifier = identifier(value["id"]),
                                      .title = string(value["title"]),
                                      .date = date(value["date"]),
                                      .time = time,
                                      .description = string(value["description"]),
                                      .repeat = enumeration<core::EventRepeat>(
                                          value["repeat"], {"None", "Daily", "Weekly", "Monthly"})});
}

core::ClientEventOccurrence occurrence(const QJsonObject& value)
{
    core::ClientEventOccurrence result{event(object(value["event"])), date(value["occurrence_date"])};
    require(value == Payload::occurrence(result.event, result.occurrenceDate));
    return result;
}

core::HabitProgress habit(const QJsonObject& value)
{
    const auto fields = object(value["habit"]);
    core::Habit parsed{.identifier = identifier(fields["id"]),
                       .name = string(fields["name"]),
                       .target = number(fields["target"]),
                       .unit = string(fields["unit"]),
                       .period = enumeration<core::HabitPeriod>(fields["period"], {"Day", "Week"})};
    return checked(value, core::HabitProgress{checked(fields, parsed), number(value["progress"])});
}

core::GeoCoordinates coordinates(const QJsonValue& value)
{
    const auto fields = object(value);
    return checked(fields, core::GeoCoordinates{number(fields["latitude"]), number(fields["longitude"])});
}

core::Reminder reminder(const QJsonObject& value)
{
    const auto kind = string(value["kind"]);
    const auto numberId = identifier(value["id"]);
    const auto title = string(value["title"]);
    core::Reminder result;
    if (kind == "Recurring")
    {
        result = core::RecurringReminder{
            numberId, title, moment(value["due_at"]),
            std::chrono::minutes{integer(value["interval_minutes"], 1, std::numeric_limits<int>::max())}};
    }
    else
    {
        const auto status = enumeration<core::ReminderStatus>(value["status"], {"Pending", "Fired", "Missed"});
        if (kind == "Timed")
        {
            result = core::TimedReminder{numberId, title, moment(value["due_at"]), status};
        }
        else
        {
            require(kind == "Place");
            result = core::PlaceReminder{
                numberId, title, string(value["place"]),
                value["coordinates"].isNull() ? std::nullopt : std::optional{coordinates(value["coordinates"])},
                status};
        }
    }
    return checked(value, result);
}

core::Chat chat(const QJsonObject& value)
{
    return checked(value, core::Chat{.identifier = identifier(value["id"]), .title = string(value["title"])});
}

core::ChatMessage message(const QJsonObject& value)
{
    return checked(value,
                   core::ChatMessage{.identifier = identifier(value["id"]),
                                     .chatIdentifier = identifier(value["chat_id"]),
                                     .role = enumeration<core::MessageRole>(value["role"], {"User", "Assistant"}),
                                     .text = string(value["text"]),
                                     .createdAt = moment(value["created_at"])});
}

core::UserProfileFact fact(const QJsonObject& value)
{
    std::vector<std::string> tags;
    require(value["tags"].isArray());
    for (const auto& tag : value["tags"].toArray())
    {
        tags.push_back(string(tag));
    }
    std::optional<core::ProfileFactSource> source;
    if (!value["source"].isNull())
    {
        const auto fields = object(value["source"]);
        source =
            checked(fields, core::ProfileFactSource{identifier(fields["chat_id"]), identifier(fields["message_id"]),
                                                    optionalId(fields["mood_id"])});
    }
    return checked(value,
                   core::UserProfileFact{.text = string(value["text"]),
                                         .tags = tags,
                                         .alwaysInclude = boolean(value["always_include"]),
                                         .identifier = identifier(value["id"]),
                                         .source = source,
                                         .replacesFactIdentifier = optionalId(value["replaces_fact_id"]),
                                         .supersededByFactIdentifier = optionalId(value["superseded_by_fact_id"])});
}

core::MoodEntry mood(const QJsonObject& value)
{
    return checked(
        value, core::MoodEntry{identifier(value["id"]), moment(value["recorded_at"]),
                               enumeration<core::MoodScore>(value["score"], {"Negative", "Neutral", "Positive"}, -1),
                               string(value["note"])});
}

core::RouteCalculation route(const QJsonObject& value)
{
    return checked(
        value, core::RouteCalculation{.identifier = identifier(value["id"]),
                                      .createdAt = moment(value["created_at"]),
                                      .originName = string(value["origin_name"]),
                                      .destinationName = string(value["destination_name"]),
                                      .origin = coordinates(value["origin"]),
                                      .destination = coordinates(value["destination"]),
                                      .mode = enumeration<core::TravelMode>(value["mode"], {"Foot", "Driving", "Bike"}),
                                      .distanceMeters = number(value["distance_meters"]),
                                      .durationSeconds = number(value["duration_seconds"]),
                                      .approximate = boolean(value["approximate"])});
}

template <typename ValueType> std::int64_t itemId(const ValueType& value)
{
    if constexpr (std::is_same_v<ValueType, core::Reminder>)
    {
        return std::visit([](const auto& item) { return item.identifier; }, value);
    }
    else if constexpr (std::is_same_v<ValueType, core::HabitProgress>)
    {
        return value.habit.identifier;
    }
    else if constexpr (std::is_same_v<ValueType, core::ClientEventOccurrence>)
    {
        return value.event.identifier;
    }
    else
    {
        return value.identifier;
    }
}

template <typename Parser> auto array(const QJsonValue& value, Parser parser)
{
    using ItemType = decltype(parser(QJsonObject{}));
    require(value.isArray());
    std::vector<ItemType> result;
    std::set<std::int64_t> identifiers;
    for (const auto& entry : value.toArray())
    {
        auto parsed = parser(object(entry));
        require(identifiers.insert(itemId(parsed)).second);
        result.push_back(std::move(parsed));
    }
    return result;
}

void fields(const QJsonObject& value, std::initializer_list<const char*> names)
{
    require(value.size() == static_cast<qsizetype>(names.size()));
    for (const auto* name : names)
    {
        require(value.contains(QLatin1String{name}));
    }
}

ClientWireRequest validated(ClientWireRequest request)
{
    require(std::holds_alternative<QByteArray>(
        ProtocolCodec::encode(ProtocolRequest{"validate", request.method, request.params})));
    return request;
}

QJsonValue optionalSearch(const std::optional<std::string>& value)
{
    return value ? QJsonValue{QString::fromStdString(*value)} : QJsonValue{QJsonValue::Null};
}
} // namespace

ClientWireRequest ClientProtocol::request(const core::ClientReadRequest& request)
{
    using Kind = core::ClientReadKind;
    switch (request.kind)
    {
    case Kind::Snapshot:
        return {"get_all", {}};
    case Kind::Chats:
        return {"chat.list", {}};
    case Kind::Messages:
        return validated({"chat.messages",
                          {{"chat_id", Payload::identifier(request.chatIdentifier)},
                           {"after_message_id", request.afterMessageIdentifier
                                                    ? QJsonValue{Payload::identifier(*request.afterMessageIdentifier)}
                                                    : QJsonValue{QJsonValue::Null}},
                           {"limit", request.limit}}});
    case Kind::CalendarDay:
        return validated({"calendar.day", {{"date", Payload::date(request.date)}}});
    case Kind::CalendarMonth:
        return validated({"calendar.month",
                          {{"year", static_cast<int>(request.month.year())},
                           {"month", static_cast<int>(static_cast<unsigned>(request.month.month()))}}});
    case Kind::Profile:
        return {"profile.get", {}};
    case Kind::Moods:
        return validated({"mood.history", {{"days", request.days}}});
    case Kind::Routes:
        return {"route.history", {}};
    }
    throw std::invalid_argument{"Неизвестный вид клиентского чтения."};
}

ClientWireRequest ClientProtocol::request(const core::ClientAction& action)
{
    return validated(std::visit(
        [](const auto& value) -> ClientWireRequest
        {
            using ActionType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<ActionType, core::ClientSetTaskDone>)
            {
                return {"task.set_done",
                        {{"task_id", Payload::identifier(value.taskIdentifier)}, {"done", value.done}}};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientSendMessage>)
            {
                return {"chat.send",
                        {{"chat_id", Payload::identifier(value.chatIdentifier)},
                         {"text", QString::fromStdString(value.text)},
                         {"search_query", optionalSearch(value.searchQuery)}}};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientRetryMessage>)
            {
                return {"chat.retry",
                        {{"chat_id", Payload::identifier(value.chatIdentifier)},
                         {"expected_user_message_id", Payload::identifier(value.userMessageIdentifier)},
                         {"search_query", optionalSearch(value.searchQuery)}}};
            }
            else
            {
                return {"chat.create", {}};
            }
        },
        action));
}

void ClientProtocol::validateActionResult(const core::ClientAction& action, const QJsonObject& value)
{
    std::visit(
        [value](const auto& action)
        {
            using ActionType = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<ActionType, core::ClientSetTaskDone>)
            {
                fields(value, {"task_id"});
                require(identifier(value["task_id"]) == action.taskIdentifier);
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientCreateChat>)
            {
                fields(value, {"chat_id"});
                static_cast<void>(identifier(value["chat_id"]));
            }
            else
            {
                fields(value, {"chat_id", "user_message_id", "message_id", "text", "tone", "mood_id",
                               "profile_fact_ids", "search_sources"});
                require(identifier(value["chat_id"]) == action.chatIdentifier);
                const auto userId = identifier(value["user_message_id"]);
                require(identifier(value["message_id"]) > userId &&
                        !QString::fromStdString(string(value["text"])).trimmed().isEmpty());
                if constexpr (std::is_same_v<ActionType, core::ClientRetryMessage>)
                {
                    require(userId == action.userMessageIdentifier);
                }
                if (!value["tone"].isNull())
                {
                    static_cast<void>(
                        enumeration<core::MoodScore>(value["tone"], {"Negative", "Neutral", "Positive"}, -1));
                }
                static_cast<void>(optionalId(value["mood_id"]));
                require(value["profile_fact_ids"].isArray() && value["search_sources"].isArray());
                std::set<std::int64_t> identifiers;
                for (const auto& item : value["profile_fact_ids"].toArray())
                {
                    require(identifiers.insert(identifier(item)).second);
                }
                for (const auto& item : value["search_sources"].toArray())
                {
                    const auto source = object(item);
                    fields(source, {"title", "url", "snippet"});
                    static_cast<void>(string(source["title"]));
                    static_cast<void>(string(source["snippet"]));
                    const QUrl url{QString::fromStdString(string(source["url"]))};
                    require(url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty() &&
                            (url.scheme() == "http" || url.scheme() == "https"));
                }
            }
        },
        action);
}

core::ClientReadResult ClientProtocol::decode(const core::ClientReadRequest& request, const QJsonObject& value)
{
    static_cast<void>(ClientProtocol::request(request));
    using Kind = core::ClientReadKind;
    switch (request.kind)
    {
    case Kind::Snapshot:
    {
        fields(value, {"server_time", "time_zone", "main_chat_id", "tasks", "reminders", "habits", "events_today"});
        const auto zone = string(value["time_zone"]);
        require(QTimeZone::isTimeZoneIdAvailable(QByteArray::fromStdString(zone)));
        return core::ClientSnapshot{moment(value["server_time"]),
                                    zone,
                                    identifier(value["main_chat_id"]),
                                    array(value["tasks"], task),
                                    array(value["reminders"], reminder),
                                    array(value["habits"], habit),
                                    array(value["events_today"], occurrence)};
    }
    case Kind::Chats:
    {
        fields(value, {"chats", "main_chat_id"});
        core::ClientChatList result{array(value["chats"], chat), identifier(value["main_chat_id"])};
        require(std::ranges::any_of(result.chats, [identifier = result.mainChatIdentifier](const auto& item)
                                    { return item.identifier == identifier; }));
        return result;
    }
    case Kind::Messages:
    {
        fields(value, {"chat_id", "messages", "next_after_message_id", "has_more"});
        core::ClientMessagePage result{identifier(value["chat_id"]), request.afterMessageIdentifier,
                                       array(value["messages"], message), optionalId(value["next_after_message_id"]),
                                       boolean(value["has_more"])};
        require(result.chatIdentifier == request.chatIdentifier &&
                result.messages.size() <= static_cast<std::size_t>(request.limit));
        auto previous = request.afterMessageIdentifier.value_or(0);
        for (const auto& item : result.messages)
        {
            require(item.chatIdentifier == request.chatIdentifier && item.identifier > previous);
            previous = item.identifier;
        }
        require(result.nextAfterMessageIdentifier == (previous == 0 ? std::nullopt : std::optional{previous}));
        require(!result.hasMore || !result.messages.empty());
        return result;
    }
    case Kind::CalendarDay:
    {
        fields(value, {"date", "events"});
        core::ClientCalendarDay result{date(value["date"]), array(value["events"], occurrence)};
        require(result.date == request.date);
        for (const auto& item : result.events)
        {
            require(item.occurrenceDate == request.date);
        }
        return result;
    }
    case Kind::CalendarMonth:
    {
        fields(value, {"year", "month", "days"});
        core::ClientCalendarMonth result{std::chrono::year{integer(value["year"], 1, 9999)} /
                                             static_cast<unsigned>(integer(value["month"], 1, 12)),
                                         {}};
        require(result.month == request.month && value["days"].isArray());
        const auto last = static_cast<int>(
            static_cast<unsigned>(std::chrono::year_month_day_last{result.month / std::chrono::last}.day()));
        unsigned previous{0};
        for (const auto& item : value["days"].toArray())
        {
            const auto day = static_cast<unsigned>(integer(item, 1, last));
            require(day > previous);
            result.days.push_back(day);
            previous = day;
        }
        return result;
    }
    case Kind::Profile:
        fields(value, {"facts"});
        return core::ClientProfile{array(value["facts"], fact)};
    case Kind::Moods:
        fields(value, {"entries"});
        return core::ClientMoods{array(value["entries"], mood)};
    case Kind::Routes:
    {
        fields(value, {"recent", "statistics"});
        const auto stats = object(value["statistics"]);
        const auto count = [](const QJsonValue& item)
        {
            const auto value = number(item);
            require(value >= 0 && value <= 9007199254740991.0 && value == std::floor(value) &&
                    value <= static_cast<double>(std::numeric_limits<std::size_t>::max()));
            return static_cast<std::size_t>(value);
        };
        core::ClientRoutes result{
            array(value["recent"], route),
            checked(stats, core::RouteCalculationStatistics{
                               count(stats["routed_count"]), count(stats["approximate_count"]),
                               number(stats["planned_distance_meters"]), number(stats["planned_duration_seconds"])})};
        require(result.recent.size() <= 5);
        return result;
    }
    }
    throw std::invalid_argument{"Неизвестный вид клиентского результата."};
}
} // namespace msa::infra
