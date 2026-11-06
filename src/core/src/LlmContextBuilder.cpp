#include "msa/core/LlmContextBuilder.h"

#include "Utf8Text.h"

#include <algorithm>
#include <iterator>
#include <string_view>

namespace msa::core {
namespace {

constexpr std::string_view kFactsHeading = "\n\nФакты профиля:";
constexpr std::string_view kFactPrefix = "\n• ";
constexpr std::string_view kDailyHeading = "\n\nСводка дня:\n";

[[nodiscard]] bool appendWithinBudget(std::string& destination, const std::string& addition,
    std::size_t& remaining)
{
    const auto count = text::charCount(addition);
    if (count > remaining) {
        return false;
    }
    destination += addition;
    remaining -= count;

    return true;
}

}  // namespace

LlmContextResult LlmContextBuilder::build(const LlmContextInput& input, std::size_t maxChars) const
{
    if (text::trim(input.systemPrompt).empty() || text::trim(input.currentQuestion).empty()) {
        return LlmContextRejection{"Укажите непустые инструкции и текущий вопрос."};
    }
    auto remaining = maxChars;
    const auto promptCount = text::charCount(input.systemPrompt);
    if (promptCount > remaining) {
        return LlmContextRejection{"Инструкции и текущий вопрос не помещаются в общий бюджет."};
    }
    remaining -= promptCount;
    const auto questionCount = text::charCount(input.currentQuestion);
    if (questionCount > remaining) {
        return LlmContextRejection{"Инструкции и текущий вопрос не помещаются в общий бюджет."};
    }
    remaining -= questionCount;

    auto history = input.history;
    std::ranges::sort(history, {}, &ChatMessage::identifier);
    std::optional<std::int64_t> previousId;
    for (const auto& message : history) {
        if (message.identifier <= 0 || previousId == message.identifier
            || message.chatIdentifier != history.front().chatIdentifier
            || (message.role != MessageRole::User && message.role != MessageRole::Assistant)) {
            return LlmContextRejection{"История должна содержать уникальные сообщения одного чата с допустимыми ролями."};
        }
        previousId = message.identifier;
    }
    if (input.currentMessageId) {
        const auto current = std::ranges::find(history, *input.currentMessageId, &ChatMessage::identifier);
        if (current == history.end() || current->role != MessageRole::User
            || current->text != input.currentQuestion) {
            return LlmContextRejection{"ID текущего вопроса не соответствует сохранённому сообщению пользователя."};
        }
        history.erase(current, history.end());
    }

    LlmContext context;
    std::string systemText = input.systemPrompt;
    std::size_t includedFacts = 0;
    for (const auto& fact : input.selectedFacts) {
        if (text::trim(fact).empty()) {
            return LlmContextRejection{"Выбранный факт профиля не должен быть пустым."};
        }
    }
    for (const auto& fact : input.selectedFacts) {
        const auto addition = (includedFacts == 0 ? std::string{kFactsHeading} : std::string{})
            + std::string{kFactPrefix} + fact;
        if (!appendWithinBudget(systemText, addition, remaining)) {
            break;
        }
        ++includedFacts;
    }
    context.omittedFacts = input.selectedFacts.size() - includedFacts;
    if (!input.dailyContext.empty()) {
        context.dailyContextIncluded = appendWithinBudget(systemText,
            std::string{kDailyHeading} + input.dailyContext, remaining);
    }
    context.messages.push_back(LlmMessage{.role = LlmRole::System, .text = systemText});

    auto first = history.end();
    while (first != history.begin()) {
        const auto previous = std::prev(first);
        const auto count = text::charCount(previous->text);
        if (count > remaining) {
            break;
        }
        remaining -= count;
        first = previous;
    }
    context.omittedHistoryMessages = static_cast<std::size_t>(std::distance(history.begin(), first));
    for (auto message = first; message != history.end(); ++message) {
        context.messages.push_back(LlmMessage{
            .role = message->role == MessageRole::User ? LlmRole::User : LlmRole::Assistant,
            .text = message->text});
    }
    context.messages.push_back(LlmMessage{.role = LlmRole::User, .text = input.currentQuestion});
    context.usedChars = maxChars - remaining;

    return context;
}

}  // namespace msa::core

