#include "msa/core/ConversationService.h"

#include "msa/core/LlmContextBuilder.h"
#include "msa/core/MentorPrompt.h"
#include "msa/core/UserProfileFactSelector.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace msa::core {

ConversationService::ConversationService(const Clock& clock, ChatService& chat,
    UserProfileService& profile, const DailyContextService& daily, LlmClient& client,
    MoodService& moods, EveningCheckInService& checkIn, EveningCheckInRepository& checkIns,
    TransactionRunner& transactions)
    : m_clock{clock}, m_chat{chat}, m_profile{profile}, m_daily{daily}, m_client{client},
      m_moods{moods}, m_checkIn{checkIn}, m_checkIns{checkIns}, m_transactions{transactions}
{
}

std::optional<PendingEveningCheckIn> ConversationService::currentPendingCheckIn()
{
    const auto pending = m_checkIns.pendingCheckIn();
    const auto dayStart = std::chrono::floor<std::chrono::days>(m_clock.now());
    const auto today = std::chrono::year_month_day{std::chrono::sys_days{dayStart.time_since_epoch()}};
    if (pending && pending->date != today) {
        m_checkIns.savePendingCheckIn(std::nullopt);
        return std::nullopt;
    }

    return pending;
}

std::optional<std::int64_t> ConversationService::offerEveningCheckIn()
{
    static_cast<void>(currentPendingCheckIn());
    const auto message = m_checkIn.checkInIfNeeded();
    if (!message) {
        return std::nullopt;
    }
    const auto result = m_chat.addMessage(*message, MessageRole::Assistant);
    if (const auto* rejection = std::get_if<ChatRejection>(&result)) {
        throw std::runtime_error{rejection->reason};
    }
    const auto identifier = std::get<std::int64_t>(result);
    const auto date = m_checkIns.lastDate();
    if (!date) {
        throw std::runtime_error{"Не сохранена дата вечернего опроса"};
    }
    m_checkIns.savePendingCheckIn(PendingEveningCheckIn{
        .chatIdentifier = m_chat.currentChatId(), .messageIdentifier = identifier, .date = *date});

    return identifier;
}

ConversationResult ConversationService::sendMessage(const std::string& message, std::size_t maxChars)
{
    const auto result = m_chat.addMessage(message, MessageRole::User);
    if (const auto* rejection = std::get_if<ChatRejection>(&result)) {
        return ConversationRejection{rejection->reason};
    }

    return retryLastMessage(maxChars);
}

ConversationResult ConversationService::retryLastMessage(std::size_t maxChars)
{
    const auto pending = currentPendingCheckIn();
    const auto history = m_chat.loadMessages();
    if (history.empty() || history.back().role != MessageRole::User) {
        return ConversationRejection{"Нет сообщения пользователя для повторной обработки."};
    }
    const auto& current = history.back();
    const bool sameChat = pending && pending->chatIdentifier == m_chat.currentChatId();
    bool awaiting = false;
    if (sameChat) {
        const auto firstUser = std::ranges::find_if(history, [pending](const ChatMessage& message) {
            return message.identifier > pending->messageIdentifier && message.role == MessageRole::User;
        });
        awaiting = firstUser != history.end() && firstUser->identifier == current.identifier;
    }
    const MentorPrompt prompt;
    const UserProfileFactSelector selector;
    const auto selected = selector.selectFacts(m_profile.profile(), current.text);
    std::vector<std::string> selectedTexts;
    for (const auto& fact : selected) {
        selectedTexts.push_back("[Факт ID=" + std::to_string(fact.identifier) + "]\n" + fact.text);
    }
    const LlmContextInput input{.systemPrompt = prompt.build(awaiting), .currentQuestion = current.text,
        .selectedFacts = selectedTexts,
        .dailyContext = m_daily.dailyContext(), .history = history, .currentMessageId = current.identifier};
    const LlmContextBuilder builder;
    const auto context = builder.build(input, maxChars);
    if (const auto* rejection = std::get_if<LlmContextRejection>(&context)) {
        return ConversationRejection{rejection->reason};
    }
    const auto& built = std::get<LlmContext>(context);
    LlmRequest request{.messages = built.messages, .awaitingEveningCheckIn = awaiting};
    const auto includedCount = selected.size() - built.omittedFacts;
    for (std::size_t index = 0; index < includedCount; ++index) {
        request.profileFactIdentifiers.push_back(selected[index].identifier);
    }
    const auto result = m_client.complete(request);
    // Ожидание могло истечь, пока модель готовила ответ.
    const auto activePending = currentPendingCheckIn();
    const bool matchingPending = sameChat && activePending
        && activePending->chatIdentifier == pending->chatIdentifier
        && activePending->messageIdentifier == pending->messageIdentifier
        && activePending->date == pending->date;
    if (const auto* failure = std::get_if<LlmFailure>(&result)) {
        return *failure;
    }
    const auto& response = std::get<LlmResponse>(result);
    if (response.moodRecording
        && response.moodRecording->reason == MoodRecordingReason::EveningCheckInReply
        && !matchingPending) {
        return ConversationRejection{"Ожидание вечернего ответа завершено. Повторите запрос."};
    }
    ConversationReply reply{.text = response.reply, .tone = response.tone};
    try {
        m_transactions.transaction([this, &reply, &response, &current, matchingPending]() {
            // Отказ также должен откатить уже сделанные записи, а не выйти из callback успешно.
            if (const auto rejection = m_profile.validateExtractedFacts(response.profileFacts)) {
                throw ConversationRejection{rejection->reason};
            }
            if (response.moodRecording) {
                const auto& proposal = *response.moodRecording;
                const auto saved = m_moods.logMood(proposal.score, proposal.note);
                if (const auto* rejection = std::get_if<MoodRejection>(&saved)) {
                    throw ConversationRejection{rejection->reason};
                }
                reply.moodIdentifier = std::get<std::int64_t>(saved);
                reply.text += "\n\nНастроение сохранено в дневник.";
            }
            const auto imported = m_profile.rememberExtractedFacts(response.profileFacts, ProfileFactSource{
                .chatIdentifier = current.chatIdentifier, .messageIdentifier = current.identifier,
                .moodIdentifier = reply.moodIdentifier});
            if (const auto* rejection = std::get_if<UserProfileRejection>(&imported)) {
                throw ConversationRejection{rejection->reason};
            }
            const auto& added = std::get<std::vector<UserProfileFact>>(imported);
            if (!added.empty()) {
                reply.text += "\n\nСохранено в памяти:";
                for (const auto& fact : added) {
                    reply.profileFactIdentifiers.push_back(fact.identifier);
                    reply.text += "\n• " + fact.text;
                }
            }
            const auto saved = m_chat.addMessage(reply.text, MessageRole::Assistant);
            if (const auto* rejection = std::get_if<ChatRejection>(&saved)) {
                throw std::runtime_error{rejection->reason};
            }
            reply.messageIdentifier = std::get<std::int64_t>(saved);
            if (matchingPending) {
                m_checkIns.savePendingCheckIn(std::nullopt);
            }
        });
    } catch (const ConversationRejection& rejection) {
        return rejection;
    }

    return reply;
}

}  // namespace msa::core

