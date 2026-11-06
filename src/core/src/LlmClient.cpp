#include "msa/core/LlmClient.h"

#include "Utf8Text.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>

namespace msa::core {
namespace {

[[nodiscard]] bool validScore(MoodScore score)
{
    return score == MoodScore::Negative || score == MoodScore::Neutral
        || score == MoodScore::Positive;
}

[[nodiscard]] std::optional<std::string> requestError(const LlmRequest& request)
{
    constexpr std::size_t kMinimumMessageCount = 2;
    if (request.messages.size() < kMinimumMessageCount
        || request.messages.front().role != LlmRole::System
        || request.messages.back().role != LlmRole::User) {
        return "Укажите инструкции System и текущий вопрос User в конце запроса.";
    }
    for (std::size_t index = 0; index < request.messages.size(); ++index) {
        const auto& message = request.messages[index];
        if (text::trim(message.text).empty()) {
            return "Сообщения запроса не должны быть пустыми.";
        }
        if (index > 0 && message.role != LlmRole::User && message.role != LlmRole::Assistant) {
            return "В истории допустимы только роли User и Assistant.";
        }
    }
    std::set<std::int64_t> identifiers;
    for (const auto identifier : request.profileFactIdentifiers) {
        if (identifier <= 0 || !identifiers.insert(identifier).second) {
            return "ID фактов контекста должны быть положительными и уникальными.";
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> responseError(
    const LlmResponse& response, const LlmRequest& request)
{
    if (text::trim(response.reply).empty()) {
        return "Модель вернула пустой ответ. Повторите запрос.";
    }
    if (response.tone && !validScore(*response.tone)) {
        return "Модель вернула неизвестную оценку тона.";
    }
    std::set<std::int64_t> replaced;
    for (const auto& fact : response.profileFacts) {
        if (text::trim(fact.text).empty()) {
            return "Модель вернула пустой факт профиля.";
        }
        if (fact.replacesFactIdentifier) {
            const auto target = *fact.replacesFactIdentifier;
            if (std::ranges::find(request.profileFactIdentifiers, target) == request.profileFactIdentifiers.end()
                || !replaced.insert(target).second) {
                return "Модель предложила повторную замену или исправление факта вне контекста.";
            }
        }
    }
    if (!response.moodRecording) {
        return std::nullopt;
    }
    const auto& proposal = *response.moodRecording;
    if (!validScore(proposal.score)) {
        return "Модель вернула неизвестную оценку для дневника.";
    }
    if (proposal.reason != MoodRecordingReason::ExplicitRequest
        && proposal.reason != MoodRecordingReason::EveningCheckInReply) {
        return "Модель вернула неизвестную причину записи настроения.";
    }
    if (proposal.reason == MoodRecordingReason::EveningCheckInReply
        && !request.awaitingEveningCheckIn) {
        return "Ответ на вечерний опрос получен, когда опрос не ожидается.";
    }

    return std::nullopt;
}

}  // namespace

LlmClient::~LlmClient() = default;

LlmResult LlmClient::complete(const LlmRequest& request)
{
    if (const auto reason = requestError(request)) {
        return LlmFailure{.kind = LlmFailureKind::InvalidRequest, .reason = *reason};
    }
    auto result = requestCompletion(request);
    if (auto* response = std::get_if<LlmResponse>(&result)) {
        if (const auto reason = responseError(*response, request)) {
            return LlmFailure{.kind = LlmFailureKind::InvalidResponse, .reason = *reason};
        }
        if (response->moodRecording) {
            response->moodRecording->note = request.messages.back().text;
        }
    } else if (auto* failure = std::get_if<LlmFailure>(&result); text::trim(failure->reason).empty()) {
        failure->reason = "Не удалось получить ответ модели. Повторите запрос.";
    }

    return result;
}

}  // namespace msa::core

