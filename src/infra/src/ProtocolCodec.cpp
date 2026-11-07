#include <msa/infra/ProtocolCodec.h>

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTime>

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace msa::infra
{
namespace
{
constexpr int kRequestIdLength{64};
constexpr int kHistoryPageLimit{100};
constexpr int kMaximumYear{9999};
constexpr int kMonthsPerYear{12};
constexpr double kLatitudeLimit{90.0};
constexpr double kLongitudeLimit{180.0};

bool matches(const QString& text, const QString& pattern)
{
    return QRegularExpression(pattern).match(text).hasMatch();
}

bool nonblank(const QJsonValue& value)
{
    return value.isString() && !value.toString().trimmed().isEmpty();
}

bool requestIdValid(const QJsonValue& value)
{
    return value.isString() && value.toString().size() <= kRequestIdLength &&
           matches(value.toString(), QStringLiteral("\\A[A-Za-z0-9_-]+\\z"));
}

bool integer(const QJsonValue& value, double minimum, double maximum)
{
    if (!value.isDouble())
    {
        return false;
    }
    const auto number = value.toDouble();

    return std::isfinite(number) && std::floor(number) == number && number >= minimum && number <= maximum;
}

bool positiveNumber(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() > 0.0;
}

bool oneOf(const QJsonValue& value, const QString& choices)
{
    return value.isString() && choices.split('|').contains(value.toString());
}

bool exactFields(const QJsonObject& object, const QStringList& fields)
{
    return object.size() == fields.size() &&
           std::ranges::all_of(fields, [object](const QString& field) { return object.contains(field); });
}

bool date(const QJsonValue& value)
{
    const auto text = value.toString();

    return value.isString() && matches(text, QStringLiteral("\\A[0-9]{4}-[0-9]{2}-[0-9]{2}\\z")) &&
           QDate::fromString(text, QStringLiteral("yyyy-MM-dd")).isValid();
}

bool time(const QJsonValue& value)
{
    const auto text = value.toString();

    return value.isString() && matches(text, QStringLiteral("\\A[0-9]{2}:[0-9]{2}\\z")) &&
           QTime::fromString(text, QStringLiteral("HH:mm")).isValid();
}

bool localDateTime(const QJsonValue& value)
{
    if (!value.isString())
    {
        return false;
    }
    const auto text = value.toString();

    return text.size() == 19 && text.at(10) == 'T' && date(text.left(10)) &&
           matches(text.mid(11), QStringLiteral("\\A[0-9]{2}:[0-9]{2}:[0-9]{2}\\z")) &&
           QTime::fromString(text.mid(11), QStringLiteral("HH:mm:ss")).isValid();
}

bool coordinates(const QJsonValue& value)
{
    if (!value.isObject())
    {
        return false;
    }
    const auto object = value.toObject();
    if (!exactFields(object, {"latitude", "longitude"}))
    {
        return false;
    }
    const auto latitude = object.value("latitude");
    const auto longitude = object.value("longitude");

    return latitude.isDouble() && longitude.isDouble() && std::isfinite(latitude.toDouble()) &&
           std::isfinite(longitude.toDouble()) && std::abs(latitude.toDouble()) <= kLatitudeLimit &&
           std::abs(longitude.toDouble()) <= kLongitudeLimit;
}

enum class Field
{
    Text,
    String,
    Id,
    NullableId,
    Boolean,
    Date,
    NullableDate,
    NullableTime,
    DateTime,
    Positive,
    PositiveInteger,
    Index,
    Year,
    Month,
    Limit,
    Coordinates,
    NullableCoordinates,
    Search,
    Tags,
    Priority,
    Repeat,
    Period,
    Score,
    Mode,
    Origin
};
using Schema = QMap<QString, Field>;

bool validField(const QJsonValue& value, Field field)
{
    switch (field)
    {
    case Field::Text:
        return nonblank(value);
    case Field::String:
        return value.isString();
    case Field::Id:
        return ProtocolCodec::isEntityId(value);
    case Field::NullableId:
        return value.isNull() || ProtocolCodec::isEntityId(value);
    case Field::Boolean:
        return value.isBool();
    case Field::Date:
        return date(value);
    case Field::NullableDate:
        return value.isNull() || date(value);
    case Field::NullableTime:
        return value.isNull() || time(value);
    case Field::DateTime:
        return localDateTime(value);
    case Field::Positive:
        return positiveNumber(value);
    case Field::PositiveInteger:
        return integer(value, 1, std::numeric_limits<int>::max());
    case Field::Index:
        return integer(value, 0, std::numeric_limits<int>::max());
    case Field::Year:
        return integer(value, 1, kMaximumYear);
    case Field::Month:
        return integer(value, 1, kMonthsPerYear);
    case Field::Limit:
        return integer(value, 1, kHistoryPageLimit);
    case Field::Coordinates:
        return coordinates(value);
    case Field::NullableCoordinates:
        return value.isNull() || coordinates(value);
    case Field::Search:
        return value.isNull() || nonblank(value);
    case Field::Tags:
    {
        if (!value.isArray())
        {
            return false;
        }
        const auto array = value.toArray();

        return std::ranges::all_of(array, [](const QJsonValue& tag) { return nonblank(tag); });
    }
    case Field::Priority:
        return oneOf(value, "Low|Normal|High");
    case Field::Repeat:
        return oneOf(value, "None|Daily|Weekly|Monthly");
    case Field::Period:
        return oneOf(value, "Day|Week");
    case Field::Score:
        return oneOf(value, "Negative|Neutral|Positive");
    case Field::Mode:
        return oneOf(value, "Foot|Bike|Driving");
    case Field::Origin:
        return nonblank(value) || coordinates(value);
    }

    return false;
}

bool validObject(const QJsonObject& object, const Schema& schema)
{
    if (!exactFields(object, schema.keys()))
    {
        return false;
    }

    return std::ranges::all_of(schema.keys(), [object, schema](const QString& key)
                               { return validField(object.value(key), schema.value(key)); });
}

const QMap<QString, Schema>& methods()
{
    static const QMap<QString, Schema> catalog{
        {"get_all", {}},
        {"ping", {}},
        {"chat.list", {}},
        {"chat.create", {}},
        {"profile.get", {}},
        {"route.history", {}},
        {"task.create", {{"title", Field::Text}, {"due_date", Field::NullableDate}, {"priority", Field::Priority}}},
        {"task.update",
         {{"task_id", Field::Id},
          {"title", Field::Text},
          {"due_date", Field::NullableDate},
          {"priority", Field::Priority}}},
        {"task.set_done", {{"task_id", Field::Id}, {"done", Field::Boolean}}},
        {"task.delete", {{"task_id", Field::Id}}},
        {"event.create",
         {{"title", Field::Text},
          {"date", Field::Date},
          {"time", Field::NullableTime},
          {"description", Field::String},
          {"repeat", Field::Repeat}}},
        {"event.update",
         {{"event_id", Field::Id},
          {"title", Field::Text},
          {"date", Field::Date},
          {"time", Field::NullableTime},
          {"description", Field::String},
          {"repeat", Field::Repeat}}},
        {"event.delete", {{"event_id", Field::Id}}},
        {"calendar.month", {{"year", Field::Year}, {"month", Field::Month}}},
        {"calendar.day", {{"date", Field::Date}}},
        {"habit.create",
         {{"name", Field::Text}, {"target", Field::Positive}, {"unit", Field::String}, {"period", Field::Period}}},
        {"habit.log", {{"habit_id", Field::Id}, {"value", Field::Positive}}},
        {"habit.delete", {{"habit_id", Field::Id}}},
        {"reminder.create", {}},
        {"reminder.delete", {{"reminder_id", Field::Id}}},
        {"reminder.location.prepare", {{"reminder_id", Field::Id}, {"reference", Field::Coordinates}}},
        {"reminder.location.confirm", {{"proposal_id", Field::Id}, {"candidate_index", Field::Index}}},
        {"reminder.location.cancel", {{"proposal_id", Field::Id}}},
        {"chat.messages", {{"chat_id", Field::Id}, {"after_message_id", Field::NullableId}, {"limit", Field::Limit}}},
        {"chat.send", {{"chat_id", Field::Id}, {"text", Field::Text}, {"search_query", Field::Search}}},
        {"chat.retry",
         {{"chat_id", Field::Id}, {"expected_user_message_id", Field::Id}, {"search_query", Field::Search}}},
        {"profile.remember", {{"text", Field::Text}, {"tags", Field::Tags}, {"always_include", Field::Boolean}}},
        {"mood.history", {{"days", Field::PositiveInteger}}},
        {"mood.record", {{"score", Field::Score}, {"note", Field::String}}},
        {"route.build",
         {{"origin", Field::Origin},
          {"destination", Field::Text},
          {"reference", Field::NullableCoordinates},
          {"mode", Field::Mode}}},
        {"weather.get", {{"coordinates", Field::Coordinates}}},
        {"location.update", {{"coordinates", Field::Coordinates}}},
        {"command.execute",
         {{"text", Field::Text}, {"chat_id", Field::Id}, {"reference", Field::NullableCoordinates}}}};

    return catalog;
}

bool validParams(const QString& method, const QJsonObject& params)
{
    if (method == "reminder.create")
    {
        const auto kind = params.value("kind");
        auto content = params;
        content.remove("kind");
        if (kind == QJsonValue("Timed"))
        {
            return validObject(content, {{"title", Field::Text}, {"due_at", Field::DateTime}});
        }
        if (kind == QJsonValue("Recurring"))
        {
            return validObject(content, {{"title", Field::Text}, {"interval_minutes", Field::PositiveInteger}});
        }
        if (kind == QJsonValue("Place"))
        {
            return validObject(content, {{"title", Field::Text}, {"place", Field::Text}});
        }

        return false;
    }
    if (!validObject(params, methods().value(method)))
    {
        return false;
    }
    if (method == "route.build")
    {
        return params.value("origin").isObject() == params.value("reference").isNull();
    }

    return true;
}

bool errorCode(const QJsonValue& value)
{
    return oneOf(value, "unauthorized|unsupported_version|invalid_frame|invalid_request|unknown_method|"
                        "validation_failed|not_found|conflict|stale_proposal|busy|location_required|"
                        "search_unavailable|search_invalid_response|llm_unavailable|llm_invalid_response|"
                        "context_budget_exceeded|storage_error|response_too_large");
}

bool validEvent(const QString& name, const QJsonObject& data)
{
    if (name == "chat.changed")
    {
        return validObject(data, {{"chat_id", Field::Id}});
    }
    if (name == "assistant.message")
    {
        return validObject(data, {{"chat_id", Field::Id}, {"message_id", Field::Id}});
    }
    if (name == "reminder.due")
    {
        return validObject(data, {{"reminder_id", Field::Id}, {"title", Field::Text}});
    }
    if (name == "event.warning")
    {
        return exactFields(data, {"event_id", "occurrence_date", "kind", "text"}) &&
               ProtocolCodec::isEntityId(data.value("event_id")) && date(data.value("occurrence_date")) &&
               oneOf(data.value("kind"), "BeforeStart|DayBefore|OnDay") && nonblank(data.value("text"));
    }
    if (name != "state.changed" || !exactFields(data, {"scopes"}) || !data.value("scopes").isArray())
    {
        return false;
    }
    const auto scopes = data.value("scopes").toArray();

    return std::ranges::all_of(
        scopes, [](const QJsonValue& scope)
        { return oneOf(scope, "tasks|reminders|events|habits|profile|moods|routes|chats|messages"); });
}

QJsonObject toObject(const ProtocolMessage& message)
{
    return std::visit(
        [](const auto& item) -> QJsonObject
        {
            using MessageType = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<MessageType, ProtocolAuth>)
            {
                return {
                    {"type", "auth"}, {"request_id", item.requestId}, {"version", item.version}, {"token", item.token}};
            }
            else if constexpr (std::is_same_v<MessageType, ProtocolRequest>)
            {
                return {{"type", "request"},
                        {"request_id", item.requestId},
                        {"method", item.method},
                        {"params", item.params}};
            }
            else if constexpr (std::is_same_v<MessageType, ProtocolSuccess>)
            {
                return {{"type", "response"}, {"request_id", item.requestId}, {"ok", true}, {"result", item.result}};
            }
            else if constexpr (std::is_same_v<MessageType, ProtocolFailure>)
            {
                return {{"type", "response"},
                        {"request_id", item.requestId},
                        {"ok", false},
                        {"error", QJsonObject{{"code", item.code},
                                              {"message", item.message},
                                              {"effect", item.effect},
                                              {"user_message_id", item.userMessageId ? QJsonValue(*item.userMessageId)
                                                                                     : QJsonValue(QJsonValue::Null)}}}};
            }
            else if constexpr (std::is_same_v<MessageType, ProtocolEvent>)
            {
                return {{"type", "event"},
                        {"server_instance_id", item.serverInstanceId},
                        {"event_id", item.eventId},
                        {"name", item.name},
                        {"data", item.data}};
            }
            else
            {
                return {{"type", "protocol_error"},
                        {"error", QJsonObject{{"code", item.code}, {"message", item.message}}}};
            }
        },
        message);
}
} // namespace

bool ProtocolCodec::isEntityId(const QJsonValue& value)
{
    if (!value.isString())
    {
        return false;
    }
    const auto text = value.toString();
    const auto maximum = QStringLiteral("9223372036854775807");

    return matches(text, QStringLiteral("\\A[1-9][0-9]*\\z")) &&
           (text.size() < maximum.size() || (text.size() == maximum.size() && text <= maximum));
}

ProtocolDecodeResult ProtocolCodec::decode(QByteArrayView frame)
{
    const ProtocolDecodeError invalidFrame{"invalid_frame", std::nullopt, true};
    if (frame.empty() || frame.size() > kProtocolMaxFrameBytes || frame.startsWith(QByteArrayView("\xEF\xBB\xBF", 3)))
    {
        return invalidFrame;
    }
    auto decoder = QStringDecoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString decoded = decoder(frame);
    if (decoder.hasError())
    {
        return invalidFrame;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(decoded.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return invalidFrame;
    }
    const auto object = document.object();
    const auto idValue = object.value("request_id");
    const auto requestId = requestIdValid(idValue) ? std::optional<QString>(idValue.toString()) : std::nullopt;
    const ProtocolDecodeError invalidRequest{"invalid_request", requestId, !requestId.has_value()};
    const auto type = object.value("type").toString();
    if (type == "auth")
    {
        if (!requestId || !exactFields(object, {"type", "request_id", "version", "token"}) ||
            !integer(object.value("version"), 0, std::numeric_limits<int>::max()) || !nonblank(object.value("token")))
        {
            return invalidRequest;
        }

        return ProtocolMessage{
            ProtocolAuth{*requestId, object.value("version").toInt(), object.value("token").toString()}};
    }
    if (type == "request")
    {
        if (!requestId || !exactFields(object, {"type", "request_id", "method", "params"}) ||
            !nonblank(object.value("method")) || !object.value("params").isObject())
        {
            return invalidRequest;
        }
        const auto method = object.value("method").toString();
        if (!methods().contains(method))
        {
            return ProtocolDecodeError{"unknown_method", requestId, false};
        }
        const auto params = object.value("params").toObject();
        if (!validParams(method, params))
        {
            return invalidRequest;
        }

        return ProtocolMessage{ProtocolRequest{*requestId, method, params}};
    }
    if (type == "response")
    {
        if (!requestId || !object.value("ok").isBool())
        {
            return invalidRequest;
        }
        if (object.value("ok").toBool())
        {
            if (!exactFields(object, {"type", "request_id", "ok", "result"}) || !object.value("result").isObject())
            {
                return invalidRequest;
            }

            return ProtocolMessage{ProtocolSuccess{*requestId, object.value("result").toObject()}};
        }
        if (!exactFields(object, {"type", "request_id", "ok", "error"}) || !object.value("error").isObject())
        {
            return invalidRequest;
        }
        const auto error = object.value("error").toObject();
        if (!exactFields(error, {"code", "message", "effect", "user_message_id"}) || !errorCode(error.value("code")) ||
            !nonblank(error.value("message")) || !oneOf(error.value("effect"), "none|user_saved|unknown"))
        {
            return invalidRequest;
        }
        const auto userId = error.value("user_message_id");
        const auto effect = error.value("effect").toString();
        if (effect == "user_saved" ? !isEntityId(userId) : !userId.isNull())
        {
            return invalidRequest;
        }

        return ProtocolMessage{
            ProtocolFailure{*requestId, error.value("code").toString(), error.value("message").toString(), effect,
                            userId.isNull() ? std::nullopt : std::optional<QString>(userId.toString())}};
    }
    if (type == "event")
    {
        if (!exactFields(object, {"type", "server_instance_id", "event_id", "name", "data"}) ||
            !nonblank(object.value("server_instance_id")) || !nonblank(object.value("event_id")) ||
            !object.value("data").isObject() ||
            !validEvent(object.value("name").toString(), object.value("data").toObject()))
        {
            return invalidRequest;
        }

        return ProtocolMessage{ProtocolEvent{object.value("server_instance_id").toString(),
                                             object.value("event_id").toString(), object.value("name").toString(),
                                             object.value("data").toObject()}};
    }
    if (type == "protocol_error")
    {
        if (!exactFields(object, {"type", "error"}) || !object.value("error").isObject())
        {
            return invalidRequest;
        }
        const auto error = object.value("error").toObject();
        if (!exactFields(error, {"code", "message"}) || !errorCode(error.value("code")) ||
            !nonblank(error.value("message")))
        {
            return invalidRequest;
        }

        return ProtocolMessage{ProtocolError{error.value("code").toString(), error.value("message").toString()}};
    }

    return invalidRequest;
}

std::variant<QByteArray, ProtocolDecodeError> ProtocolCodec::encode(const ProtocolMessage& message)
{
    auto bytes = QJsonDocument(toObject(message)).toJson(QJsonDocument::Compact);
    if (bytes.size() > kProtocolMaxFrameBytes)
    {
        return ProtocolDecodeError{"response_too_large", std::nullopt, false};
    }
    const auto checked = decode(bytes);
    if (const auto* error = std::get_if<ProtocolDecodeError>(&checked))
    {
        return *error;
    }
    bytes.append('\n');

    return bytes;
}

ProtocolFrameBatch ProtocolFrameDecoder::append(QByteArrayView bytes)
{
    ProtocolFrameBatch batch;
    if (m_failed)
    {
        batch.invalidFrame = true;
        return batch;
    }
    while (!bytes.empty())
    {
        const auto newline = bytes.indexOf('\n');
        const auto count = newline < 0 ? bytes.size() : newline;
        if (count > kProtocolMaxFrameBytes - m_pending.size() || (newline >= 0 && m_pending.isEmpty() && count == 0))
        {
            m_failed = true;
            m_pending.clear();
            batch.invalidFrame = true;
            break;
        }
        m_pending.append(bytes.data(), count);
        if (newline < 0)
        {
            break;
        }
        batch.frames.push_back(std::move(m_pending));
        m_pending.clear();
        bytes = bytes.sliced(count + 1);
    }

    return batch;
}

void ProtocolFrameDecoder::reset()
{
    m_pending.clear();
    m_failed = false;
}
} // namespace msa::infra
