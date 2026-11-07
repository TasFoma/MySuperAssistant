#include "msa/infra/LlamaClient.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace msa::infra {
namespace {

using namespace std::chrono_literals;
constexpr int kSuccessStatus = 200;

class FakeReply final : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest& request, QByteArray body, int status,
              NetworkError error, bool stall, bool& aborted, QObject* parent);
    void abort() override;

protected:
    qint64 readData(char* data, qint64 maximum) override;

private:
    QByteArray m_body{};
    qint64 m_offset{};
    bool& m_aborted;
};

FakeReply::FakeReply(const QNetworkRequest& request, QByteArray body, int status,
                     NetworkError error, bool stall, bool& aborted, QObject* parent) :
    QNetworkReply{parent}, m_body{std::move(body)}, m_aborted{aborted}
{
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::PostOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    if (error != NoError) {
        setError(error, "SECRET technical response");
    }
    open(QIODevice::ReadOnly);
    if (!stall) {
        QTimer::singleShot(0, this, [this]() {
            setFinished(true);
            emit finished();
        });
    }
}

void FakeReply::abort()
{
    m_aborted = true;
    setError(OperationCanceledError, "SECRET canceled");
    setFinished(true);
    emit finished();
}

qint64 FakeReply::readData(char* data, qint64 maximum)
{
    const auto count = std::min(maximum, static_cast<qint64>(m_body.size()) - m_offset);
    if (count <= 0) {
        return -1;
    }
    std::memcpy(data, m_body.constData() + m_offset, static_cast<std::size_t>(count));
    m_offset += count;

    return count;
}

class FakeNetworkManager final : public QNetworkAccessManager {
public:
    QByteArray response{};
    int status{kSuccessStatus};
    QNetworkReply::NetworkError error{QNetworkReply::NoError};
    bool stall{false};
    bool aborted{false};
    int calls{};
    QNetworkRequest lastRequest{};
    Operation lastOperation{};
    QByteArray lastBody{};

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                QIODevice* outgoing) override;
};

QNetworkReply* FakeNetworkManager::createRequest(Operation operation, const QNetworkRequest& request,
                                                QIODevice* outgoing)
{
    ++calls;
    lastRequest = request;
    lastOperation = operation;
    lastBody = outgoing ? outgoing->readAll() : QByteArray{};

    return new FakeReply{request, response, status, error, stall, aborted, this};
}

[[nodiscard]] LlamaClientSettings settings()
{
    return {.completionUrl = QUrl{"http://model.invalid/v1/chat/completions"},
            .model = "test-model", .apiKey = "SECRET", .timeout = 1000ms, .maxResponseTokens = 900};
}

[[nodiscard]] core::LlmRequest request()
{
    return {.messages = {{core::LlmRole::System, "Инструкции"},
                         {core::LlmRole::User, "Раньше"},
                         {core::LlmRole::Assistant, "Ответ"},
                         {core::LlmRole::User, "  Запиши: радуюсь\nподробности  "}},
            .profileFactIdentifiers = {7}};
}

[[nodiscard]] QJsonObject content()
{
    return {{"reply", "  Привет\nмир  "}, {"tone", QJsonValue::Null},
            {"mood_recording", QJsonValue::Null}, {"profile_facts", QJsonArray{}}};
}

[[nodiscard]] QByteArray envelope(const QByteArray& text, const QString& finishReason = "stop")
{
    return QJsonDocument{QJsonObject{
        {"choices", QJsonArray{QJsonObject{
            {"finish_reason", finishReason},
            {"message", QJsonObject{{"role", "assistant"}, {"content", QString::fromUtf8(text)}}}}}}}}.toJson();
}

[[nodiscard]] QByteArray envelope(const QJsonObject& object)
{
    return envelope(QJsonDocument{object}.toJson(QJsonDocument::Compact));
}

void expectFailure(const core::LlmResult& result, core::LlmFailureKind kind)
{
    ASSERT_TRUE(std::holds_alternative<core::LlmFailure>(result));
    const auto& failure = std::get<core::LlmFailure>(result);
    EXPECT_EQ(failure.kind, kind);
    EXPECT_FALSE(failure.reason.empty());
    EXPECT_EQ(failure.reason.find("SECRET"), std::string::npos);
}

TEST(LlamaClientTest, PostsFullMessagesAndConfiguredLimitsWithoutRealNetwork)
{
    FakeNetworkManager manager;
    manager.response = envelope(content());
    LlamaClient client{manager, settings()};
    const auto input = request();
    const auto result = client.complete(input);
    ASSERT_TRUE(std::holds_alternative<core::LlmResponse>(result));
    EXPECT_EQ(std::get<core::LlmResponse>(result).reply, "  Привет\nмир  ");
    EXPECT_EQ(manager.calls, 1);
    EXPECT_EQ(manager.lastOperation, QNetworkAccessManager::PostOperation);
    EXPECT_EQ(manager.lastRequest.url(), settings().completionUrl);
    EXPECT_EQ(manager.lastRequest.rawHeader("Authorization"), "Bearer SECRET");
    EXPECT_EQ(manager.lastRequest.header(QNetworkRequest::ContentTypeHeader).toString(), "application/json");
    EXPECT_EQ(manager.lastRequest.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
              QNetworkRequest::ManualRedirectPolicy);
    const auto body = QJsonDocument::fromJson(manager.lastBody).object();
    EXPECT_EQ(body["max_tokens"].toInt(), 900);
    EXPECT_FALSE(body["stream"].toBool(true));
    EXPECT_EQ(body["model"].toString(), "test-model");
    EXPECT_EQ(body["response_format"].toObject()["type"].toString(), "json_object");
    EXPECT_FALSE(body.contains("stop"));
    const auto messages = body["messages"].toArray();
    ASSERT_EQ(messages.size(), input.messages.size());
    const std::vector<QString> roles{"system", "user", "assistant", "user"};
    for (qsizetype index = 0; index < messages.size(); ++index) {
        const auto message = messages[index].toObject();
        EXPECT_EQ(message["role"].toString(), roles[static_cast<std::size_t>(index)]);
        EXPECT_EQ(message["content"].toString().toStdString(), input.messages[static_cast<std::size_t>(index)].text);
    }
}

TEST(LlamaClientTest, AllowsServerModelAndNoAuthentication)
{
    FakeNetworkManager manager;
    manager.response = envelope(content());
    auto options = settings();
    options.model.clear();
    options.apiKey.clear();
    LlamaClient client{manager, options};
    EXPECT_TRUE(std::holds_alternative<core::LlmResponse>(client.complete(request())));
    EXPECT_FALSE(manager.lastRequest.hasRawHeader("Authorization"));
    EXPECT_FALSE(QJsonDocument::fromJson(manager.lastBody).object().contains("model"));
}

TEST(LlamaClientTest, ParsesMoodAndVersionedFactsKeepingOriginalNote)
{
    FakeNetworkManager manager;
    auto object = content();
    object["tone"] = "Negative";
    object["mood_recording"] = QJsonObject{{"score", "Positive"}, {"reason", "ExplicitRequest"}};
    object["profile_facts"] = QJsonArray{
        QJsonObject{{"text", "Новый факт"}, {"replaces_fact_id", QJsonValue::Null}},
        QJsonObject{{"text", "Исправление"}, {"replaces_fact_id", 7}}};
    manager.response = envelope(object);
    LlamaClient client{manager, settings()};
    const auto input = request();
    const auto result = client.complete(input);
    ASSERT_TRUE(std::holds_alternative<core::LlmResponse>(result));
    const auto& response = std::get<core::LlmResponse>(result);
    EXPECT_EQ(response.tone, core::MoodScore::Negative);
    ASSERT_TRUE(response.moodRecording);
    EXPECT_EQ(response.moodRecording->score, core::MoodScore::Positive);
    EXPECT_EQ(response.moodRecording->note, input.messages.back().text);
    ASSERT_EQ(response.profileFacts.size(), 2);
    EXPECT_FALSE(response.profileFacts[0].replacesFactIdentifier);
    EXPECT_EQ(response.profileFacts[1].replacesFactIdentifier, 7);
}

TEST(LlamaClientTest, RejectsWrongTypesMissingExtraFieldsAndUnknownEnums)
{
    std::vector<QJsonObject> invalid;
    for (const auto& key : content().keys()) {
        auto object = content();
        object.remove(key);
        invalid.push_back(object);
    }
    for (const auto& replacement : std::vector<QJsonObject>{
             {{"extra", true}}, {{"reply", 42}}, {{"reply", " \n "}}, {{"tone", "Unknown"}},
             {{"tone", 1}}, {{"profile_facts", QJsonValue::Null}},
             {{"mood_recording", QJsonObject{{"score", "Positive"}}}},
             {{"mood_recording", QJsonObject{{"score", "Unknown"}, {"reason", "ExplicitRequest"}}}},
             {{"mood_recording", QJsonObject{{"score", "Positive"}, {"reason", "Unknown"}}}},
             {{"mood_recording", QJsonObject{{"score", "Positive"}, {"reason", "ExplicitRequest"}, {"note", "secret"}}}}}) {
        auto object = content();
        for (auto iterator = replacement.begin(); iterator != replacement.end(); ++iterator) {
            object[iterator.key()] = iterator.value();
        }
        invalid.push_back(object);
    }
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    for (const auto& object : invalid) {
        SCOPED_TRACE(QJsonDocument{object}.toJson().constData());
        manager.response = envelope(object);
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
    EXPECT_EQ(manager.calls, invalid.size());
}

TEST(LlamaClientTest, RejectsInvalidFactIdentifiersAndObjects)
{
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    const std::vector<QJsonValue> identifiers{0, -1, 1.5, "7", true, 1e30, 8};
    for (const auto& identifier : identifiers) {
        auto object = content();
        object["profile_facts"] = QJsonArray{QJsonObject{{"text", "Факт"}, {"replaces_fact_id", identifier}}};
        manager.response = envelope(object);
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
    for (const auto& fact : QJsonArray{QJsonValue::Null, QJsonObject{{"text", "Факт"}},
             QJsonObject{{"text", 42}, {"replaces_fact_id", QJsonValue::Null}},
             QJsonObject{{"text", " "}, {"replaces_fact_id", QJsonValue::Null}},
             QJsonObject{{"text", "Факт"}, {"replaces_fact_id", QJsonValue::Null}, {"tags", QJsonArray{}}}}) {
        auto object = content();
        object["profile_facts"] = QJsonArray{fact};
        manager.response = envelope(object);
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
}

TEST(LlamaClientTest, RejectsUnexpectedEveningReplyAndDuplicateReplacement)
{
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    auto object = content();
    object["mood_recording"] = QJsonObject{{"score", "Neutral"}, {"reason", "EveningCheckInReply"}};
    manager.response = envelope(object);
    expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    auto input = request();
    input.awaitingEveningCheckIn = true;
    EXPECT_TRUE(std::holds_alternative<core::LlmResponse>(client.complete(input)));
    object = content();
    const QJsonObject fact{{"text", "Факт"}, {"replaces_fact_id", 7}};
    object["profile_facts"] = QJsonArray{fact, fact};
    manager.response = envelope(object);
    expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
}

TEST(LlamaClientTest, RejectsMalformedOrDecoratedModelJson)
{
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    for (const auto& text : std::vector<QByteArray>{"", "{", "[]", "null", "```json\n{}\n```",
                                                   "{} trailing", "{}{}"}) {
        manager.response = envelope(text);
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
}

TEST(LlamaClientTest, RejectsBrokenEnvelopesAndTruncatedOrToolResponses)
{
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    for (const auto& body : std::vector<QByteArray>{"", "{", "[]", "{}", "{\"choices\":[]}",
             "{\"choices\":[null]}", "{\"choices\":[{},{}]}"}) {
        manager.response = body;
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
    for (const auto& reason : {"length", "tool_calls", "content_filter", "unknown"}) {
        manager.response = envelope(QJsonDocument{content()}.toJson(), reason);
        expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
    }
    auto root = QJsonDocument::fromJson(envelope(content())).object();
    auto choices = root["choices"].toArray();
    auto choice = choices[0].toObject();
    auto message = choice["message"].toObject();
    message["content"] = QJsonValue::Null;
    choice["message"] = message;
    choices[0] = choice;
    root["choices"] = choices;
    manager.response = QJsonDocument{root}.toJson();
    expectFailure(client.complete(request()), core::LlmFailureKind::InvalidResponse);
}

TEST(LlamaClientTest, ReportsHttpAndNetworkFailuresWithoutSecretsOrRetries)
{
    FakeNetworkManager manager;
    manager.response = "SECRET server error";
    LlamaClient client{manager, settings()};
    for (const auto status : {301, 401, 429, 500, 503}) {
        manager.status = status;
        expectFailure(client.complete(request()), core::LlmFailureKind::Unavailable);
    }
    manager.status = kSuccessStatus;
    manager.error = QNetworkReply::ConnectionRefusedError;
    expectFailure(client.complete(request()), core::LlmFailureKind::Unavailable);
    EXPECT_EQ(manager.calls, 6);
}

TEST(LlamaClientTest, AbortsOnDeadlineAndCanMakeNextExplicitRequest)
{
    FakeNetworkManager manager;
    manager.stall = true;
    auto options = settings();
    options.timeout = 10ms;
    LlamaClient client{manager, options};
    expectFailure(client.complete(request()), core::LlmFailureKind::Unavailable);
    EXPECT_TRUE(manager.aborted);
    EXPECT_EQ(manager.calls, 1);
    manager.stall = false;
    manager.response = envelope(content());
    EXPECT_TRUE(std::holds_alternative<core::LlmResponse>(client.complete(request())));
    EXPECT_EQ(manager.calls, 2);
}

TEST(LlamaClientTest, InvalidCoreRequestDoesNotAccessTransport)
{
    FakeNetworkManager manager;
    LlamaClient client{manager, settings()};
    expectFailure(client.complete({}), core::LlmFailureKind::InvalidRequest);
    EXPECT_EQ(manager.calls, 0);
}

TEST(LlamaClientTest, PreservesInt64FactIdentifierWithoutDoubleRounding)
{
    FakeNetworkManager manager;
    auto object = content();
    constexpr auto kIdentifier = std::numeric_limits<qint64>::max();
    object["profile_facts"] = QJsonArray{
        QJsonObject{{"text", "Исправление"}, {"replaces_fact_id", QJsonValue{kIdentifier}}}};
    manager.response = envelope(object);
    auto input = request();
    input.profileFactIdentifiers = {kIdentifier};
    LlamaClient client{manager, settings()};
    const auto result = client.complete(input);
    ASSERT_TRUE(std::holds_alternative<core::LlmResponse>(result));
    const auto& facts = std::get<core::LlmResponse>(result).profileFacts;
    ASSERT_EQ(facts.size(), 1);
    EXPECT_EQ(facts.front().replacesFactIdentifier, kIdentifier);
}

TEST(LlamaClientTest, RejectsReentryWithoutSendingAnotherRequest)
{
    FakeNetworkManager manager;
    manager.response = envelope(content());
    LlamaClient client{manager, settings()};
    std::optional<core::LlmResult> nested;
    QTimer::singleShot(0, &manager, [&client, &nested]() { nested = client.complete(request()); });
    EXPECT_TRUE(std::holds_alternative<core::LlmResponse>(client.complete(request())));
    ASSERT_TRUE(nested);
    expectFailure(*nested, core::LlmFailureKind::Unavailable);
    EXPECT_EQ(manager.calls, 1);
}

TEST(LlamaClientTest, RejectsInvalidSettingsBeforeNetworkAccess)
{
    FakeNetworkManager manager;
    for (const auto& address : {"", "file:///tmp/model", "http://", "http://user:SECRET@host/path",
                               "http://host/path?SECRET", "http://host/path#fragment"}) {
        auto options = settings();
        options.completionUrl = QUrl{address};
        EXPECT_THROW((LlamaClient{manager, options}), std::invalid_argument);
    }
    auto options = settings();
    options.timeout = 0ms;
    EXPECT_THROW((LlamaClient{manager, options}), std::invalid_argument);
    options = settings();
    options.maxResponseTokens = 0;
    EXPECT_THROW((LlamaClient{manager, options}), std::invalid_argument);
    options = settings();
    options.apiKey = "SECRET\r\nheader";
    EXPECT_THROW((LlamaClient{manager, options}), std::invalid_argument);
    EXPECT_EQ(manager.calls, 0);
}

}  // namespace
}  // namespace msa::infra

