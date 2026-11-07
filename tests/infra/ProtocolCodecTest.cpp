#include <msa/infra/ProtocolCodec.h>
#include <msa/infra/ProtocolPayload.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <vector>

namespace msa::infra
{
namespace
{
QByteArray request(const QString& method, const QJsonObject& params)
{
    return QJsonDocument(
               QJsonObject{{"type", "request"}, {"request_id", "req-1"}, {"method", method}, {"params", params}})
        .toJson(QJsonDocument::Compact);
}

QJsonObject json(const QByteArray& bytes)
{
    return QJsonDocument::fromJson(bytes).object();
}

void expectInvalid(const QByteArray& bytes, const QString& code, bool closeConnection)
{
    const auto result = ProtocolCodec::decode(bytes);
    ASSERT_TRUE(std::holds_alternative<ProtocolDecodeError>(result));
    const auto& error = std::get<ProtocolDecodeError>(result);
    EXPECT_EQ(error.code, code);
    EXPECT_EQ(error.closeConnection, closeConnection);
}

const std::vector<std::pair<QString, QJsonObject>>& examples()
{
    static const std::vector<std::pair<QString, QJsonObject>> values{
        {"get_all", {}},
        {"ping", {}},
        {"chat.list", {}},
        {"chat.create", {}},
        {"profile.get", {}},
        {"route.history", {}},
        {"task.create", json(R"({"title":"  Текст\nцеликом  ","due_date":null,"priority":"Normal"})")},
        {"task.update",
         json(R"({"task_id":"9223372036854775807","title":"Текст","due_date":"2028-02-29","priority":"High"})")},
        {"task.set_done", json(R"({"task_id":"1","done":false})")},
        {"task.delete", json(R"({"task_id":"1"})")},
        {"event.create",
         json(R"({"title":"Текст","date":"2026-11-08","time":null,"description":"","repeat":"Weekly"})")},
        {"event.update",
         json(
             R"({"event_id":"1","title":"Текст","date":"2026-11-08","time":"23:59","description":"текст","repeat":"None"})")},
        {"event.delete", json(R"({"event_id":"1"})")},
        {"calendar.month", json(R"({"year":2026,"month":11})")},
        {"calendar.day", json(R"({"date":"2026-11-08"})")},
        {"habit.create", json(R"({"name":"Вода","target":1.5,"unit":"л","period":"Day"})")},
        {"habit.log", json(R"({"habit_id":"1","value":0.5})")},
        {"habit.delete", json(R"({"habit_id":"1"})")},
        {"reminder.create", json(R"({"kind":"Timed","title":"Текст","due_at":"2026-11-08T00:00:00"})")},
        {"reminder.create", json(R"({"kind":"Recurring","title":"Текст","interval_minutes":1})")},
        {"reminder.create", json(R"({"kind":"Place","title":"Текст","place":"Дом"})")},
        {"reminder.delete", json(R"({"reminder_id":"1"})")},
        {"reminder.location.prepare", json(R"({"reminder_id":"1","reference":{"latitude":0,"longitude":0}})")},
        {"reminder.location.confirm", json(R"({"proposal_id":"1","candidate_index":0})")},
        {"reminder.location.cancel", json(R"({"proposal_id":"1"})")},
        {"chat.messages", json(R"({"chat_id":"1","after_message_id":null,"limit":100})")},
        {"chat.send", json(R"({"chat_id":"1","text":"Вопрос","search_query":null})")},
        {"chat.retry", json(R"({"chat_id":"1","expected_user_message_id":"2","search_query":"Поиск"})")},
        {"profile.remember", json(R"({"text":"Факт","tags":["тег","тег"],"always_include":true})")},
        {"mood.history", json(R"({"days":7})")},
        {"mood.record", json(R"({"score":"Neutral","note":""})")},
        {"route.build",
         json(R"({"origin":{"latitude":90,"longitude":-180},"destination":"Дом","reference":null,"mode":"Foot"})")},
        {"route.build",
         json(R"({"origin":"Дом","destination":"Парк","reference":{"latitude":0,"longitude":0},"mode":"Driving"})")},
        {"weather.get", json(R"({"coordinates":{"latitude":-90,"longitude":180}})")},
        {"location.update", json(R"({"coordinates":{"latitude":0,"longitude":0}})")},
        {"command.execute", json(R"({"text":"Команда","chat_id":"1","reference":null})")}};

    return values;
}
} // namespace

TEST(ProtocolCodecTest, AcceptsEveryMethodAndPreservesParametersThroughRoundTrip)
{
    for (const auto& [method, params] : examples())
    {
        SCOPED_TRACE(method.toStdString());
        const auto result = ProtocolCodec::decode(request(method, params));
        ASSERT_TRUE(std::holds_alternative<ProtocolMessage>(result));
        const auto& message = std::get<ProtocolMessage>(result);
        const auto& parsed = std::get<ProtocolRequest>(message);
        EXPECT_EQ(parsed.params, params);
        const auto encoded = ProtocolCodec::encode(message);
        ASSERT_TRUE(std::holds_alternative<QByteArray>(encoded));
        const auto bytes = std::get<QByteArray>(encoded);
        EXPECT_TRUE(bytes.endsWith('\n'));
        EXPECT_EQ(bytes.count('\n'), 1);
        EXPECT_EQ(json(bytes).value("params").toObject(), params);
    }
}

TEST(ProtocolCodecTest, RejectsMissingUnknownAndWrongTypedParametersForEveryMethod)
{
    for (const auto& [method, params] : examples())
    {
        SCOPED_TRACE(method.toStdString());
        auto extra = params;
        extra.insert("untrusted", true);
        expectInvalid(request(method, extra), "invalid_request", false);
        for (const auto& key : params.keys())
        {
            SCOPED_TRACE(key.toStdString());
            auto missing = params;
            missing.remove(key);
            expectInvalid(request(method, missing), "invalid_request", false);
            auto wrong = params;
            wrong.insert(key, QJsonObject{});
            expectInvalid(request(method, wrong), "invalid_request", false);
        }
    }
}

TEST(ProtocolCodecTest, EntityIdsPreserveInt64AndRejectNoncanonicalValues)
{
    EXPECT_TRUE(ProtocolCodec::isEntityId("1"));
    EXPECT_TRUE(ProtocolCodec::isEntityId("9223372036854775807"));
    for (const auto& text : {"0", "01", "-1", "+1", "1.0", "1e2", " 1", "1\n", "9223372036854775808", "١"})
    {
        EXPECT_FALSE(ProtocolCodec::isEntityId(text)) << text;
    }
    EXPECT_FALSE(ProtocolCodec::isEntityId(1));
    EXPECT_FALSE(ProtocolCodec::isEntityId(QJsonValue::Null));
}

TEST(ProtocolCodecTest, RejectsInvalidDatesTimesEnumsAndRanges)
{
    const std::vector<std::pair<QString, QByteArray>> cases{
        {"calendar.day", R"({"date":"2027-02-29"})"},
        {"calendar.day", R"({"date":"2026-1-01"})"},
        {"calendar.month", R"({"year":0,"month":1})"},
        {"calendar.month", R"({"year":2026,"month":13})"},
        {"calendar.month", R"({"year":2026.5,"month":1})"},
        {"task.create", R"({"title":"x","due_date":"","priority":"Normal"})"},
        {"task.create", R"({"title":"x","due_date":null,"priority":"normal"})"},
        {"task.create", R"({"title":" \n\t","due_date":null,"priority":"Normal"})"},
        {"event.create", R"({"title":"x","date":"2026-01-01","time":"24:00","description":"","repeat":"None"})"},
        {"reminder.create", R"({"kind":"Timed","title":"x","due_at":"2026-01-01T12:00:00Z"})"},
        {"reminder.create", R"({"kind":"Timed","title":"x","due_at":"2026-01-01T12:00:60"})"},
        {"reminder.create", R"({"kind":"Recurring","title":"x","interval_minutes":0})"},
        {"reminder.create", R"({"kind":"Place","title":"x","place":"Дом","coordinates":null})"},
        {"habit.log", R"({"habit_id":"1","value":0})"},
        {"chat.messages", R"({"chat_id":"1","after_message_id":null,"limit":101})"},
        {"chat.messages", R"({"chat_id":"1","after_message_id":"0","limit":1})"},
        {"chat.send", R"({"chat_id":"1","text":"x","search_query":" "})"},
        {"profile.remember", R"({"text":"x","tags":[" "],"always_include":true})"},
        {"weather.get", R"({"coordinates":{"latitude":90.1,"longitude":0}})"},
        {"weather.get", R"({"coordinates":{"latitude":0,"longitude":180.1}})"},
        {"weather.get", R"({"coordinates":{"latitude":"0","longitude":0}})"},
        {"route.build", R"({"origin":"Дом","destination":"Парк","reference":null,"mode":"Bike"})"},
        {"route.build",
         R"({"origin":{"latitude":0,"longitude":0},"destination":"Парк","reference":{"latitude":0,"longitude":0},"mode":"Bike"})"}};
    for (const auto& [method, params] : cases)
    {
        SCOPED_TRACE(params.toStdString());
        expectInvalid(request(method, json(params)), "invalid_request", false);
    }
}

TEST(ProtocolCodecTest, SeparatesUnknownMethodAndCorrelatedErrorsFromFatalErrors)
{
    expectInvalid(request("chat.select", {}), "unknown_method", false);
    auto envelope = json(request("ping", {}));
    envelope.insert("unknown", 1);
    expectInvalid(QJsonDocument(envelope).toJson(), "invalid_request", false);
    envelope.remove("unknown");
    for (const auto& identifier : {QString{}, QString("a.b"), QString("рус"), QString(65, 'a')})
    {
        envelope.insert("request_id", identifier);
        expectInvalid(QJsonDocument(envelope).toJson(), "invalid_request", true);
    }
    envelope.insert("request_id", QString(64, 'a'));
    EXPECT_TRUE(std::holds_alternative<ProtocolMessage>(ProtocolCodec::decode(QJsonDocument(envelope).toJson())));
    const auto result = ProtocolCodec::decode(request("unknown", {}));
    EXPECT_EQ(std::get<ProtocolDecodeError>(result).requestId, std::optional<QString>("req-1"));
}

TEST(ProtocolCodecTest, RoundTripsAllEnvelopes)
{
    const std::vector<ProtocolMessage> messages{
        ProtocolAuth{"auth-1", 1, "secret"},
        ProtocolSuccess{"req-1", {{"task_id", "9223372036854775807"}}},
        ProtocolFailure{"req-1", "storage_error", "Ошибка", "unknown", std::nullopt},
        ProtocolFailure{"req-1", "llm_unavailable", "Ошибка", "user_saved", QString("2")},
        ProtocolEvent{"instance", "event-1", "state.changed", {{"scopes", QJsonArray{"tasks", "messages"}}}},
        ProtocolEvent{"instance", "event-2", "chat.changed", {{"chat_id", "1"}}},
        ProtocolEvent{"instance", "event-3", "assistant.message", {{"chat_id", "1"}, {"message_id", "2"}}},
        ProtocolEvent{"instance", "event-4", "reminder.due", {{"reminder_id", "1"}, {"title", "Текст"}}},
        ProtocolEvent{
            "instance",
            "event-5",
            "event.warning",
            {{"event_id", "1"}, {"occurrence_date", "2026-11-08"}, {"kind", "BeforeStart"}, {"text", "Текст"}}},
        ProtocolError{"invalid_frame", "Неверный кадр"}};
    for (const auto& message : messages)
    {
        const auto encoded = ProtocolCodec::encode(message);
        ASSERT_TRUE(std::holds_alternative<QByteArray>(encoded));
        auto bytes = std::get<QByteArray>(encoded);
        bytes.chop(1);
        const auto decoded = ProtocolCodec::decode(bytes);
        ASSERT_TRUE(std::holds_alternative<ProtocolMessage>(decoded));
        EXPECT_EQ(std::get<ProtocolMessage>(decoded).index(), message.index());
        const auto repeated = ProtocolCodec::encode(std::get<ProtocolMessage>(decoded));
        ASSERT_TRUE(std::holds_alternative<QByteArray>(repeated));
        EXPECT_EQ(std::get<QByteArray>(repeated), std::get<QByteArray>(encoded));
    }
}

TEST(ProtocolCodecTest, RejectsContradictoryResponsesAndEvents)
{
    const std::vector<ProtocolMessage> invalid{
        ProtocolFailure{"req-1", "storage_error", "Ошибка", "user_saved", std::nullopt},
        ProtocolFailure{"req-1", "storage_error", "Ошибка", "none", QString("2")},
        ProtocolFailure{"req-1", "unknown_error", "Ошибка", "none", std::nullopt},
        ProtocolEvent{"instance", "event-1", "chat.selected", {{"chat_id", "1"}}},
        ProtocolEvent{"instance", "event-1", "state.changed", {{"scopes", QJsonArray{"unknown"}}}},
        ProtocolEvent{"instance", "event-1", "chat.changed", {{"chat_id", 1}}}};
    for (const auto& message : invalid)
    {
        EXPECT_TRUE(std::holds_alternative<ProtocolDecodeError>(ProtocolCodec::encode(message)));
    }
    expectInvalid(R"({"type":"response","request_id":"r","ok":true,"result":{},"error":{}})", "invalid_request", false);
    expectInvalid(R"({"type":"response","request_id":"r","ok":1,"result":{}})", "invalid_request", false);
    expectInvalid(R"({"type":"response","request_id":"r","ok":true,"result":[]})", "invalid_request", false);
}

TEST(ProtocolCodecTest, DefersVersionAndTokenAuthenticationToServer)
{
    const auto encoded = ProtocolCodec::encode(ProtocolAuth{"a", 2, "other-token"});
    EXPECT_TRUE(std::holds_alternative<QByteArray>(encoded));
    expectInvalid(R"({"type":"auth","request_id":"a","version":1.5,"token":"x"})", "invalid_request", false);
    expectInvalid(R"({"type":"auth","request_id":"a","version":1,"token":null})", "invalid_request", false);
}

TEST(ProtocolCodecTest, RejectsInvalidJsonUtf8BomAndNonobjects)
{
    for (const auto& bytes :
         {QByteArray{}, QByteArray("{"), QByteArray("[]"), QByteArray("null"), QByteArray("{}{}"),
          QByteArray("\xEF\xBB\xBF{}"), QByteArray("{\"text\":\"\xFF\"}"), QByteArray("{\"text\":\"\xD0")})
    {
        expectInvalid(bytes, "invalid_frame", true);
    }
}

TEST(ProtocolCodecTest, UsesLastDuplicateKeyAsDocumented)
{
    const auto result =
        ProtocolCodec::decode(R"({"type":"request","request_id":"r","method":"unknown","method":"ping","params":{}})");
    ASSERT_TRUE(std::holds_alternative<ProtocolMessage>(result));
    EXPECT_EQ(std::get<ProtocolRequest>(std::get<ProtocolMessage>(result)).method, "ping");
}

TEST(ProtocolCodecTest, BoundsIncomingAndOutgoingFramesWithoutTruncation)
{
    auto frame = request("ping", {});
    frame.append(kProtocolMaxFrameBytes - frame.size(), ' ');
    EXPECT_TRUE(std::holds_alternative<ProtocolMessage>(ProtocolCodec::decode(frame)));
    frame.append(' ');
    expectInvalid(frame, "invalid_frame", true);
    const auto encoded = ProtocolCodec::encode(ProtocolSuccess{"r", {{"text", QString(kProtocolMaxFrameBytes, 'x')}}});
    ASSERT_TRUE(std::holds_alternative<ProtocolDecodeError>(encoded));
    EXPECT_EQ(std::get<ProtocolDecodeError>(encoded).code, "response_too_large");
}

TEST(ProtocolCodecTest, ReassemblesEveryPossibleByteSplitIncludingUtf8)
{
    const auto frame =
        request("chat.send", {{"chat_id", "1"}, {"text", "Привет 🌍\nстрока"}, {"search_query", QJsonValue::Null}});
    const auto wire = frame + '\n';
    for (qsizetype split = 0; split < wire.size(); ++split)
    {
        ProtocolFrameDecoder decoder;
        const auto first = decoder.append(QByteArrayView(wire).first(split));
        EXPECT_TRUE(first.frames.empty());
        EXPECT_FALSE(first.invalidFrame);
        const auto last = decoder.append(QByteArrayView(wire).sliced(split));
        ASSERT_EQ(last.frames.size(), 1U);
        EXPECT_EQ(last.frames.front(), frame);
        EXPECT_TRUE(std::holds_alternative<ProtocolMessage>(ProtocolCodec::decode(last.frames.front())));
    }
}

TEST(ProtocolCodecTest, PreservesFrameOrderAndDropsIncompleteEofOnReset)
{
    ProtocolFrameDecoder decoder;
    const auto frame = request("ping", {});
    const auto batch = decoder.append(frame + '\n' + frame + '\n' + "unfinished");
    ASSERT_EQ(batch.frames.size(), 2U);
    EXPECT_EQ(batch.frames[0], frame);
    EXPECT_EQ(batch.frames[1], frame);
    EXPECT_FALSE(batch.invalidFrame);
    decoder.reset();
    const auto fresh = decoder.append(frame + '\n');
    ASSERT_EQ(fresh.frames.size(), 1U);
    EXPECT_EQ(fresh.frames.front(), frame);
}

TEST(ProtocolCodecTest, StopsAtEmptyLineUntilResetKeepingEarlierFrames)
{
    ProtocolFrameDecoder decoder;
    const auto frame = request("ping", {});
    const auto batch = decoder.append(frame + "\n\n" + frame + '\n');
    EXPECT_TRUE(batch.invalidFrame);
    ASSERT_EQ(batch.frames.size(), 1U);
    EXPECT_EQ(batch.frames.front(), frame);
    EXPECT_TRUE(decoder.append(frame + '\n').invalidFrame);
    decoder.reset();
    EXPECT_FALSE(decoder.append(frame + '\n').invalidFrame);
}

TEST(ProtocolCodecTest, SerializesSavedObjectsWithExactWireFields)
{
    const auto moment = std::chrono::local_seconds{std::chrono::local_days{std::chrono::year{2028} / 2 / 29}} +
                        std::chrono::hours{23} + std::chrono::minutes{59} + std::chrono::seconds{58};
    core::Task task{.identifier = 9223372036854775807LL, .title = "  Текст\nцеликом  ", .createdAt = moment};
    const auto taskJson = ProtocolPayload::object(task);
    EXPECT_EQ(
        taskJson,
        json(
            R"({"id":"9223372036854775807","title":"  Текст\nцеликом  ","due_date":null,"priority":"Normal","done":false,"created_at":"2028-02-29T23:59:58"})"));
    core::Event event{.identifier = 1, .title = "Серия", .date = std::chrono::year{2028} / 2 / 1};
    const auto occurrence = ProtocolPayload::occurrence(event, std::chrono::year{2028} / 2 / 29);
    EXPECT_EQ(occurrence.value("event").toObject().value("date"), "2028-02-01");
    EXPECT_TRUE(occurrence.value("event").toObject().value("time").isNull());
    EXPECT_EQ(occurrence.value("occurrence_date"), "2028-02-29");
    event.time = std::chrono::minutes{1439};
    EXPECT_EQ(ProtocolPayload::object(event).value("time"), "23:59");
    EXPECT_EQ(ProtocolPayload::object(core::Chat{.identifier = 1, .title = "Чат"}),
              json(R"({"id":"1","title":"Чат"})"));
    const auto message = ProtocolPayload::object(core::ChatMessage{.identifier = 2,
                                                                   .chatIdentifier = 1,
                                                                   .role = core::MessageRole::Assistant,
                                                                   .text = "  Ответ\n  ",
                                                                   .createdAt = moment});
    EXPECT_EQ(message.value("text"), "  Ответ\n  ");
    EXPECT_EQ(message.value("role"), "Assistant");
    EXPECT_EQ(message.value("chat_id"), "1");
    EXPECT_EQ(
        ProtocolPayload::object(core::MoodEntry{.identifier = 1, .score = core::MoodScore::Negative}).value("score"),
        "Negative");
    EXPECT_EQ(ProtocolPayload::object(core::HabitProgress{core::Habit{.identifier = 1}, 3.5}).value("progress"), 3.5);
}

TEST(ProtocolCodecTest, SerializesReminderVariantsAndProfileNulls)
{
    const auto timed = ProtocolPayload::object(core::Reminder{core::TimedReminder{.identifier = 1}});
    EXPECT_EQ(timed.value("kind"), "Timed");
    EXPECT_EQ(timed.value("status"), "Pending");
    const auto recurring = ProtocolPayload::object(
        core::Reminder{core::RecurringReminder{.identifier = 2, .interval = std::chrono::minutes{15}}});
    EXPECT_EQ(recurring.value("kind"), "Recurring");
    EXPECT_EQ(recurring.value("interval_minutes"), 15);
    EXPECT_FALSE(recurring.contains("status"));
    const auto place = ProtocolPayload::object(core::Reminder{core::PlaceReminder{.identifier = 3}});
    EXPECT_EQ(place.value("kind"), "Place");
    EXPECT_TRUE(place.value("coordinates").isNull());
    core::UserProfileFact fact{.text = "Факт", .identifier = 1};
    const auto manual = ProtocolPayload::object(fact);
    EXPECT_TRUE(manual.value("source").isNull());
    EXPECT_TRUE(manual.value("replaces_fact_id").isNull());
    EXPECT_TRUE(manual.value("superseded_by_fact_id").isNull());
    fact.source = core::ProfileFactSource{1, 2, 3};
    fact.replacesFactIdentifier = 4;
    EXPECT_EQ(ProtocolPayload::object(fact).value("source").toObject(),
              json(R"({"chat_id":"1","message_id":"2","mood_id":"3"})"));
    EXPECT_EQ(ProtocolPayload::object(fact).value("replaces_fact_id"), "4");
}

TEST(ProtocolCodecTest, SerializesNavigationWithoutInventingExitOrWeatherValues)
{
    EXPECT_EQ(ProtocolPayload::object(core::GeoCoordinates{}), json(R"({"latitude":0,"longitude":0})"));
    const auto weather = ProtocolPayload::object(core::WeatherForecast{.weatherCode = 999});
    EXPECT_EQ(weather.value("weather_code"), 999);
    EXPECT_TRUE(weather.value("maximum_temperature_celsius").isNull());
    EXPECT_TRUE(weather.value("minimum_temperature_celsius").isNull());
    EXPECT_TRUE(weather.value("precipitation_probability_percent").isNull());
    EXPECT_TRUE(ProtocolPayload::object(core::RouteStep{}).value("exit_number").isNull());
    for (const auto mode : {core::TravelMode::Foot, core::TravelMode::Driving, core::TravelMode::Bike})
    {
        const auto route = ProtocolPayload::object(core::RouteCalculation{.identifier = 1, .mode = mode});
        EXPECT_TRUE(route.value("mode").isString());
        EXPECT_EQ(route.value("id"), "1");
    }
    EXPECT_EQ(
        ProtocolPayload::object(core::RouteCalculation{.identifier = 1, .mode = core::TravelMode::Bike}).value("mode"),
        "Bike");
    EXPECT_EQ(ProtocolPayload::object(core::RouteCalculationStatistics{}).value("routed_count"), 0);
    EXPECT_EQ(ProtocolPayload::object(core::WebSearchResult{"Заголовок", "https://example.com", "Текст"}),
              json(R"({"title":"Заголовок","url":"https://example.com","snippet":"Текст"})"));
}

TEST(ProtocolCodecTest, RefusesInvalidDomainValuesInsteadOfSilentlyEmittingNullOrWrongDates)
{
    EXPECT_THROW((void)ProtocolPayload::identifier(0), std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::date(std::chrono::year{2027} / 2 / 29), std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::localDateTime(std::chrono::local_seconds::max()), std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::object(core::GeoCoordinates{std::numeric_limits<double>::infinity(), 0}),
                 std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::object(
                     core::WeatherForecast{.temperatureCelsius = std::numeric_limits<double>::quiet_NaN()}),
                 std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::object(core::WeatherForecast{.maximumTemperatureCelsius = 10.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::object(core::Chat{.identifier = 1, .title = std::string("\xFF")}),
                 std::invalid_argument);
    EXPECT_THROW(
        (void)ProtocolPayload::object(core::Task{.identifier = 1, .priority = static_cast<core::TaskPriority>(99)}),
        std::invalid_argument);
    EXPECT_THROW((void)ProtocolPayload::object(core::RouteStep{.exitNumber = 0}), std::invalid_argument);
}

TEST(ProtocolCodecTest, BoundsUnfinishedBufferAndAcceptsExactByteLimit)
{
    ProtocolFrameDecoder decoder;
    EXPECT_FALSE(decoder.append(QByteArray(kProtocolMaxFrameBytes, 'x')).invalidFrame);
    EXPECT_TRUE(decoder.append("x").invalidFrame);
    EXPECT_TRUE(decoder.append("\n").frames.empty());
    decoder.reset();
    EXPECT_FALSE(decoder.append(QByteArray(kProtocolMaxFrameBytes, 'x')).invalidFrame);
    const auto batch = decoder.append("\n");
    ASSERT_EQ(batch.frames.size(), 1U);
    EXPECT_EQ(batch.frames.front().size(), kProtocolMaxFrameBytes);
}
} // namespace msa::infra
