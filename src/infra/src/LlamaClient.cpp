#include "msa/infra/LlamaClient.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedValueRollback>
#include <QThread>
#include <QTimer>

#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace msa::infra {
namespace {

constexpr double kTemperature = 0.6;
constexpr double kTopProbability = 0.9;
constexpr int kTopCandidates = 40;
constexpr double kRepeatPenalty = 1.1;
constexpr int kHttpOk = 200;

[[nodiscard]] core::LlmFailure invalidResponse()
{
    return {.kind = core::LlmFailureKind::InvalidResponse,
            .reason = "Модель вернула неполный ответ или неверный JSON. Повторите запрос."};
}

[[nodiscard]] bool hasKeys(const QJsonObject& object, std::initializer_list<QString> keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const auto& key : keys) {
        if (!object.contains(key)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] std::optional<core::MoodScore> parseScore(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const auto text = value.toString();
    if (text == "Negative") {
        return core::MoodScore::Negative;
    }
    if (text == "Neutral") {
        return core::MoodScore::Neutral;
    }
    if (text == "Positive") {
        return core::MoodScore::Positive;
    }

    return std::nullopt;
}

[[nodiscard]] core::LlmResult parseContent(const QByteArray& content)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidResponse();
    }
    const auto object = document.object();
    if (!hasKeys(object, {"reply", "tone", "mood_recording", "profile_facts"})
        || !object["reply"].isString() || !object["profile_facts"].isArray()) {
        return invalidResponse();
    }
    core::LlmResponse response{.reply = object["reply"].toString().toStdString()};
    if (!object["tone"].isNull()) {
        response.tone = parseScore(object["tone"]);
        if (!response.tone) {
            return invalidResponse();
        }
    }
    if (!object["mood_recording"].isNull()) {
        const auto value = object["mood_recording"];
        if (!value.isObject() || !hasKeys(value.toObject(), {"score", "reason"})) {
            return invalidResponse();
        }
        const auto mood = value.toObject();
        const auto score = parseScore(mood["score"]);
        if (!score || !mood["reason"].isString()) {
            return invalidResponse();
        }
        const auto reason = mood["reason"].toString();
        if (reason != "ExplicitRequest" && reason != "EveningCheckInReply") {
            return invalidResponse();
        }
        response.moodRecording = core::MoodRecordingProposal{
            .score = *score,
            .reason = reason == "ExplicitRequest" ? core::MoodRecordingReason::ExplicitRequest
                                                  : core::MoodRecordingReason::EveningCheckInReply};
    }
    for (const auto& value : object["profile_facts"].toArray()) {
        if (!value.isObject() || !hasKeys(value.toObject(), {"text", "replaces_fact_id"})) {
            return invalidResponse();
        }
        const auto fact = value.toObject();
        if (!fact["text"].isString()) {
            return invalidResponse();
        }
        core::ProfileFactProposal proposal{.text = fact["text"].toString().toStdString()};
        const auto identifier = fact["replaces_fact_id"];
        if (!identifier.isNull()) {
            // toInteger проверяет целочисленность и диапазон, не округляя дробь.
            const auto number = identifier.toInteger(0);
            if (!identifier.isDouble() || number <= 0) {
                return invalidResponse();
            }
            proposal.replacesFactIdentifier = number;
        }
        response.profileFacts.push_back(std::move(proposal));
    }

    return response;
}

[[nodiscard]] core::LlmResult parseEnvelope(const QByteArray& body)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidResponse();
    }
    const auto object = document.object();
    const auto choices = object["choices"];
    if (object.contains("error") || !choices.isArray() || choices.toArray().size() != 1
        || !choices.toArray().first().isObject()) {
        return invalidResponse();
    }
    const auto choice = choices.toArray().first().toObject();
    const auto message = choice["message"].toObject();
    if (choice["finish_reason"] != "stop" || message["role"] != "assistant"
        || !message["content"].isString() || message.contains("tool_calls")
        || message.contains("function_call")) {
        return invalidResponse();
    }

    return parseContent(message["content"].toString().toUtf8());
}

[[nodiscard]] QByteArray requestBody(const core::LlmRequest& request, const LlamaClientSettings& settings)
{
    QJsonArray messages;
    for (const auto& message : request.messages) {
        const auto* role = message.role == core::LlmRole::System ? "system"
            : message.role == core::LlmRole::User ? "user" : "assistant";
        messages.append(QJsonObject{{"role", role}, {"content", QString::fromStdString(message.text)}});
    }
    QJsonObject body{{"messages", messages}, {"stream", false}, {"max_tokens", settings.maxResponseTokens},
                     {"temperature", kTemperature}, {"top_p", kTopProbability},
                     {"top_k", kTopCandidates}, {"repeat_penalty", kRepeatPenalty},
                     {"response_format", QJsonObject{{"type", "json_object"}}}};
    if (!settings.model.isEmpty()) {
        body.insert("model", settings.model);
    }

    return QJsonDocument{body}.toJson(QJsonDocument::Compact);
}

}  // namespace

LlamaClient::LlamaClient(QNetworkAccessManager& manager, LlamaClientSettings settings) :
    m_manager{manager}, m_settings{std::move(settings)}
{
    const auto& address = m_settings.completionUrl;
    if (!address.isValid() || (address.scheme() != "http" && address.scheme() != "https")
        || address.host().isEmpty() || !address.userInfo().isEmpty() || address.hasQuery()
        || address.hasFragment() || m_settings.timeout.count() <= 0
        || m_settings.timeout.count() > std::numeric_limits<int>::max()
        || m_settings.maxResponseTokens <= 0 || m_settings.apiKey.contains('\r')
        || m_settings.apiKey.contains('\n')) {
        throw std::invalid_argument{"Некорректные настройки llama-server."};
    }
}

core::LlmResult LlamaClient::requestCompletion(const core::LlmRequest& request)
{
    if (!QCoreApplication::instance() || QThread::currentThread() != m_manager.thread() || m_inProgress) {
        return core::LlmFailure{.kind = core::LlmFailureKind::Unavailable,
                               .reason = "Клиент модели недоступен в текущем потоке или уже занят."};
    }
    QScopedValueRollback<bool> progress{m_inProgress, true};
    QNetworkRequest networkRequest{m_settings.completionUrl};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!m_settings.apiKey.isEmpty()) {
        networkRequest.setRawHeader("Authorization", "Bearer " + m_settings.apiKey);
    }
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    const std::unique_ptr<QNetworkReply> reply{m_manager.post(networkRequest, requestBody(request, m_settings))};
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(static_cast<int>(m_settings.timeout.count()));
    if (!reply->isFinished()) {
        loop.exec();
    }
    if (!reply->isFinished()) {
        reply->abort();
        return core::LlmFailure{.kind = core::LlmFailureKind::Unavailable,
                               .reason = "Истекло время ожидания ответа модели. Повторите запрос."};
    }
    if (reply->error() != QNetworkReply::NoError
        || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != kHttpOk) {
        return core::LlmFailure{.kind = core::LlmFailureKind::Unavailable,
                               .reason = "llama-server недоступен или отклонил запрос. Проверьте настройки сервера."};
    }

    return parseEnvelope(reply->readAll());
}

}  // namespace msa::infra

