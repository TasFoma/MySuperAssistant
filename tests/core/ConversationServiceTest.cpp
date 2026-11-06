#include "FakeClock.h"
#include "FakeLlmClient.h"
#include "InMemoryChatRepository.h"
#include "InMemoryEveningCheckInRepository.h"
#include "InMemoryEventRepository.h"
#include "InMemoryHabitRepository.h"
#include "InMemoryMoodRepository.h"
#include "InMemoryTaskRepository.h"
#include "InMemoryTransactionRunner.h"
#include "InMemoryUserProfileRepository.h"

#include "msa/core/CalendarService.h"
#include "msa/core/ConversationService.h"
#include "msa/core/HabitService.h"
#include "msa/core/MentorPrompt.h"
#include "msa/core/TaskService.h"

#include "Utf8Text.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

constexpr std::size_t kContextBudget = 20000;

class ConversationServiceTest : public ::testing::Test {
protected:
    ConversationServiceTest();
    [[nodiscard]] ConversationService conversation();
    [[nodiscard]] std::vector<MoodEntry> diary() const;

    FakeClock m_clock;
    InMemoryChatRepository m_chats{};
    ChatService m_chat;
    InMemoryUserProfileRepository m_profiles{};
    UserProfileService m_profile;
    InMemoryEventRepository m_events{};
    CalendarService m_calendar;
    InMemoryTaskRepository m_tasks{};
    TaskService m_taskService;
    InMemoryHabitRepository m_habits{};
    HabitService m_habitService;
    DailyContextService m_daily;
    FakeLlmClient m_client;
    InMemoryMoodRepository m_moods{};
    MoodService m_moodService;
    InMemoryEveningCheckInRepository m_checkIns{};
    EveningCheckInService m_checkIn;
    InMemoryTransactionRunner m_transactions;
};

ConversationServiceTest::ConversationServiceTest()
    : m_clock{std::chrono::local_days{std::chrono::year{2026} / 11 / 6} + std::chrono::hours{20}},
      m_chat{m_clock, m_chats}, m_profile{m_profiles}, m_calendar{m_events},
      m_taskService{m_clock, m_tasks}, m_habitService{m_clock, m_habits},
      m_daily{m_clock, m_calendar, m_taskService, m_habitService},
      m_client{LlmResponse{.reply = "Полный ответ", .tone = MoodScore::Neutral}},
      m_moodService{m_clock, m_moods}, m_checkIn{m_clock, m_checkIns},
      m_transactions{m_chats, m_moods, m_profiles, m_checkIns}
{
}

ConversationService ConversationServiceTest::conversation()
{
    return ConversationService{m_clock, m_chat, m_profile, m_daily, m_client,
        m_moodService, m_checkIn, m_checkIns, m_transactions};
}

std::vector<MoodEntry> ConversationServiceTest::diary() const
{
    return std::get<std::vector<MoodEntry>>(m_moodService.moodHistory());
}

TEST_F(ConversationServiceTest, CombinesPromptProfileDailySummaryAndHistoryWithoutRepeatingQuestion)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Отвечай кратко", {}, true)));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Учусь C++", {"работа"})));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Люблю чай")));
    const auto profileBefore = m_profile.profile();
    static_cast<void>(m_chat.addMessage("Прошлый вопрос", MessageRole::User));
    static_cast<void>(m_chat.addMessage("Прошлый ответ", MessageRole::Assistant));
    auto service = conversation();
    const std::string question = "  работа\n🙂  ";
    const auto result = service.sendMessage(question, kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
    const auto reply = std::get<ConversationReply>(result);
    EXPECT_EQ(reply.text, "Полный ответ");
    EXPECT_EQ(reply.tone, MoodScore::Neutral);
    EXPECT_FALSE(reply.moodIdentifier);
    EXPECT_TRUE(diary().empty());
    ASSERT_EQ(m_client.requests().size(), 1U);
    const auto& request = m_client.requests().front();
    EXPECT_FALSE(request.awaitingEveningCheckIn);
    ASSERT_EQ(request.messages.size(), 4U);
    EXPECT_NE(request.messages.front().text.find("Учитывай отказ"), std::string::npos);
    EXPECT_NE(request.messages.front().text.find("Отвечай кратко"), std::string::npos);
    EXPECT_NE(request.messages.front().text.find("Учусь C++"), std::string::npos);
    EXPECT_EQ(request.messages.front().text.find("Люблю чай"), std::string::npos);
    EXPECT_NE(request.messages.front().text.find(m_daily.dailyContext()), std::string::npos);
    EXPECT_EQ(request.messages.back().text, question);
    EXPECT_EQ(m_profile.profile().facts, profileBefore.facts);
    const auto history = m_chat.loadMessages();
    ASSERT_EQ(history.size(), 4U);
    EXPECT_EQ(history[2].text, question);
    EXPECT_EQ(history.back().identifier, reply.messageIdentifier);
    EXPECT_EQ(history.back().text, reply.text);
}

TEST_F(ConversationServiceTest, ExplicitMoodRecordingPreservesOriginalAndAddsConfirmation)
{
    m_client.setResult(LlmResponse{.reply = "Спасибо за рассказ", .tone = MoodScore::Negative,
        .moodRecording = MoodRecordingProposal{.score = MoodScore::Positive,
            .reason = MoodRecordingReason::ExplicitRequest, .note = "Сокращённая заметка"}});
    auto service = conversation();
    const std::string question = "  Запиши: радуюсь встрече\n  Все подробности 🙂  ";
    const auto reply = std::get<ConversationReply>(service.sendMessage(question, kContextBudget));
    ASSERT_TRUE(reply.moodIdentifier);
    const auto entries = diary();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().identifier, reply.moodIdentifier);
    EXPECT_EQ(entries.front().note, question);
    EXPECT_EQ(entries.front().score, MoodScore::Positive);
    EXPECT_EQ(reply.text, "Спасибо за рассказ\n\nНастроение сохранено в дневник.");
    EXPECT_EQ(m_chat.loadMessages().back().text, reply.text);
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(service.retryLastMessage(kContextBudget)));
    EXPECT_EQ(diary().size(), 1U);
    EXPECT_EQ(m_client.requests().size(), 1U);
}

TEST_F(ConversationServiceTest, PublishesOneDailyCheckInAndRestoresWaitingFromRepository)
{
    auto service = conversation();
    const auto promptId = service.offerEveningCheckIn();
    ASSERT_TRUE(promptId);
    ASSERT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_checkIns.pendingCheckIn()->chatIdentifier, m_chat.currentChatId());
    EXPECT_EQ(m_checkIns.pendingCheckIn()->messageIdentifier, promptId);
    EXPECT_FALSE(service.offerEveningCheckIn());
    ChatService restoredChat{m_clock, m_chats};
    ConversationService restarted{m_clock, restoredChat, m_profile, m_daily, m_client,
        m_moodService, m_checkIn, m_checkIns, m_transactions};
    EXPECT_FALSE(restarted.offerEveningCheckIn());
    ASSERT_EQ(m_chat.loadMessages().size(), 1U);
    EXPECT_EQ(m_chat.loadMessages().front().role, MessageRole::Assistant);
    EXPECT_NE(m_chat.loadMessages().front().text.find("дневник настроения"), std::string::npos);
    EXPECT_TRUE(m_client.requests().empty());
    static_cast<void>(restarted.sendMessage("Не хочу отвечать", kContextBudget));
    EXPECT_TRUE(m_client.requests().front().awaitingEveningCheckIn);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_TRUE(m_checkIns.lastDate());
}

TEST_F(ConversationServiceTest, RecordsFirstEveningReplyAndStopsWaitingAfterSuccess)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_client.setResult(LlmResponse{.reply = "Спасибо", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Neutral, .reason = MoodRecordingReason::EveningCheckInReply}});
    const std::string answer = "  День обычный\n  без перемен  ";
    const auto result = service.sendMessage(answer, kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
    EXPECT_TRUE(m_client.requests().front().awaitingEveningCheckIn);
    EXPECT_NE(m_client.requests().front().messages.front().text.find("awaitingEveningCheckIn=true"),
        std::string::npos);
    ASSERT_EQ(diary().size(), 1U);
    EXPECT_EQ(diary().front().note, answer);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    m_client.setResult(LlmResponse{.reply = "Дальше"});
    static_cast<void>(service.sendMessage("Следующая тема", kContextBudget));
    EXPECT_FALSE(m_client.requests().back().awaitingEveningCheckIn);
    EXPECT_EQ(diary().size(), 1U);
}

TEST_F(ConversationServiceTest, RefusalTopicChangeAndUnclearMoodCloseCheckInWithoutRecording)
{
    auto service = conversation();
    for (const auto& message : std::vector<std::string>{"Не хочу", "Поговорим о C++", "Не знаю"}) {
        ASSERT_TRUE(service.offerEveningCheckIn());
        const auto result = service.sendMessage(message, kContextBudget);
        ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
        EXPECT_TRUE(m_client.requests().back().awaitingEveningCheckIn);
        EXPECT_FALSE(m_checkIns.pendingCheckIn());
        EXPECT_TRUE(diary().empty());
        m_clock.advance(std::chrono::days{1});
    }
}

TEST_F(ConversationServiceTest, SwitchingChatsDoesNotMoveOrConsumeWaitingInAnotherChat)
{
    auto service = conversation();
    const auto originalChat = m_chat.currentChatId();
    ASSERT_TRUE(service.offerEveningCheckIn());
    const auto otherChat = m_chat.newChat();
    EXPECT_FALSE(service.offerEveningCheckIn());
    static_cast<void>(service.sendMessage("Вопрос в другом чате", kContextBudget));
    EXPECT_FALSE(m_client.requests().back().awaitingEveningCheckIn);
    ASSERT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_checkIns.pendingCheckIn()->chatIdentifier, originalChat);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_chat.switchChat(originalChat)));
    static_cast<void>(service.sendMessage("Отказ от опроса", kContextBudget));
    EXPECT_TRUE(m_client.requests().back().awaitingEveningCheckIn);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_NE(originalChat, otherChat);
}

TEST_F(ConversationServiceTest, MidnightExpiresWaitingWithoutErasingOriginalPrompt)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    const auto prompt = m_chat.loadMessages().front();
    m_clock.advance(std::chrono::hours{4});
    EXPECT_FALSE(service.offerEveningCheckIn());
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    static_cast<void>(service.sendMessage("Ответ после полуночи", kContextBudget));
    EXPECT_FALSE(m_client.requests().back().awaitingEveningCheckIn);
    EXPECT_EQ(m_chat.loadMessages().front().text, prompt.text);
    m_clock.advance(std::chrono::hours{20});
    ASSERT_TRUE(service.offerEveningCheckIn());
    ASSERT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_NE(m_checkIns.pendingCheckIn()->messageIdentifier, prompt.identifier);
}

TEST_F(ConversationServiceTest, LlmFailureRetainsWaitingAndExplicitRetryDoesNotDuplicateUser)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_client.setResult(LlmFailure{.kind = LlmFailureKind::Unavailable, .reason = "Сервис недоступен"});
    const std::string answer = "Мне хорошо";
    const auto failed = service.sendMessage(answer, kContextBudget);
    ASSERT_TRUE(std::holds_alternative<LlmFailure>(failed));
    EXPECT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_TRUE(diary().empty());
    ASSERT_EQ(m_chat.loadMessages().size(), 2U);
    const auto questionId = m_chat.loadMessages().back().identifier;
    EXPECT_EQ(m_client.requests().size(), 1U);
    m_client.setResult(LlmResponse{.reply = "Спасибо", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply}});
    auto restarted = conversation();
    const auto retry = restarted.retryLastMessage(kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(retry));
    ASSERT_EQ(m_client.requests().size(), 2U);
    EXPECT_TRUE(m_client.requests().back().awaitingEveningCheckIn);
    EXPECT_EQ(m_client.requests().back().messages.back().text, answer);
    EXPECT_EQ(m_chat.loadMessages().size(), 3U);
    EXPECT_EQ(m_chat.loadMessages()[1].identifier, questionId);
    EXPECT_EQ(diary().size(), 1U);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
}

TEST_F(ConversationServiceTest, BudgetFailureKeepsUserAndCanBeRetriedWithoutCallingLlmEarly)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    const auto failed = service.sendMessage("Ответ на опрос", 0);
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(failed));
    EXPECT_TRUE(m_client.requests().empty());
    EXPECT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_chat.loadMessages().size(), 2U);
    EXPECT_TRUE(std::holds_alternative<ConversationReply>(service.retryLastMessage(kContextBudget)));
    EXPECT_EQ(m_chat.loadMessages().size(), 3U);
    EXPECT_TRUE(m_client.requests().front().awaitingEveningCheckIn);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
}

TEST_F(ConversationServiceTest, InvalidLlmResponseDoesNotWriteDiaryOrAssistantOrCloseWaiting)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_client.setResult(LlmResponse{.reply = " ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply}});
    const auto result = service.sendMessage("Всё хорошо", kContextBudget);
    ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
    EXPECT_EQ(std::get<LlmFailure>(result).kind, LlmFailureKind::InvalidResponse);
    EXPECT_TRUE(diary().empty());
    EXPECT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_chat.loadMessages().size(), 2U);
    EXPECT_EQ(m_client.requests().size(), 1U);
}

TEST_F(ConversationServiceTest, SecondUserAfterFailedAttemptIsNotMistakenForFirstEveningReply)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_client.setResult(LlmFailure{.reason = "Ошибка"});
    static_cast<void>(service.sendMessage("Первый ответ", kContextBudget));
    EXPECT_TRUE(m_client.requests().back().awaitingEveningCheckIn);
    m_client.setResult(LlmResponse{.reply = "Другая тема"});
    EXPECT_TRUE(std::holds_alternative<ConversationReply>(service.sendMessage("Новая тема", kContextBudget)));
    EXPECT_FALSE(m_client.requests().back().awaitingEveningCheckIn);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_TRUE(diary().empty());
}

TEST_F(ConversationServiceTest, UnexpectedEveningRecordingIsRejectedAndEmptyInputIsNotSaved)
{
    auto service = conversation();
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(service.sendMessage("\xC2\xA0", kContextBudget)));
    EXPECT_TRUE(m_chat.loadMessages().empty());
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(service.retryLastMessage(kContextBudget)));
    EXPECT_TRUE(m_client.requests().empty());
    m_client.setResult(LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply}});
    const auto result = service.sendMessage("Обычный разговор", kContextBudget);
    EXPECT_TRUE(std::holds_alternative<LlmFailure>(result));
    EXPECT_TRUE(diary().empty());
    EXPECT_EQ(m_chat.loadMessages().size(), 1U);
}

/// @brief Отказ до сохранения дневника, чтобы проверить отсутствие ложного подтверждения.
class FailingMoodRepository final : public MoodRepository {
public:
    [[nodiscard]] std::int64_t add(const MoodEntry& entry) override;
    [[nodiscard]] std::vector<MoodEntry> all() const override;
};

std::int64_t FailingMoodRepository::add([[maybe_unused]] const MoodEntry& entry)
{
    throw std::runtime_error{"Ошибка дневника"};
}

std::vector<MoodEntry> FailingMoodRepository::all() const
{
    return {};
}

TEST_F(ConversationServiceTest, StorageFailurePropagatesWithoutFalseConfirmation)
{
    FailingMoodRepository repository;
    MoodService moods{m_clock, repository};
    ConversationService service{m_clock, m_chat, m_profile, m_daily, m_client, moods, m_checkIn, m_checkIns, m_transactions};
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_client.setResult(LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply}});
    EXPECT_THROW(static_cast<void>(service.sendMessage("Хорошо", kContextBudget)), std::runtime_error);
    ASSERT_EQ(m_chat.loadMessages().size(), 2U);
    EXPECT_EQ(m_chat.loadMessages().back().role, MessageRole::User);
    EXPECT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_client.requests().size(), 1U);
}

TEST_F(ConversationServiceTest, PendingRepositoryReturnsCopiesAndClearingKeepsIssueDate)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    auto copy = m_checkIns.pendingCheckIn();
    ASSERT_TRUE(copy);
    copy->chatIdentifier = 0;
    EXPECT_EQ(m_checkIns.pendingCheckIn()->chatIdentifier, m_chat.currentChatId());
    const auto date = m_checkIns.lastDate();
    m_checkIns.savePendingCheckIn(std::nullopt);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_checkIns.lastDate(), date);
    EXPECT_FALSE(service.offerEveningCheckIn());
}

TEST_F(ConversationServiceTest, AutomaticallySavesFactWithSourceWithoutCreatingMoodEntry)
{
    m_client.setResult(LlmResponse{.reply = "Поздравляю!", .tone = MoodScore::Positive,
        .profileFacts = {{.text = "  Начала работать разработчиком  "}}});
    auto service = conversation();
    const std::string original = "  Сегодня устроилась разработчиком\n  Все подробности 🙂  ";
    const auto result = service.sendMessage(original, kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
    const auto reply = std::get<ConversationReply>(result);
    ASSERT_EQ(reply.profileFactIdentifiers.size(), 1U);
    const auto profile = m_profile.profile();
    ASSERT_EQ(profile.facts.size(), 1U);
    const auto& fact = profile.facts.front();
    ASSERT_TRUE(fact.source);
    EXPECT_EQ(fact.identifier, reply.profileFactIdentifiers.front());
    EXPECT_EQ(fact.source->chatIdentifier, m_chat.currentChatId());
    EXPECT_EQ(fact.source->messageIdentifier, m_chat.loadMessages().front().identifier);
    EXPECT_FALSE(fact.source->moodIdentifier);
    EXPECT_EQ(m_chat.loadMessages().front().text, original);
    EXPECT_TRUE(diary().empty());
    EXPECT_EQ(reply.text, "Поздравляю!\n\nСохранено в памяти:\n• Начала работать разработчиком");
    EXPECT_EQ(m_chat.loadMessages().back().text, reply.text);
    EXPECT_TRUE(fact.tags.empty());
    const auto repeated = std::get<ConversationReply>(service.sendMessage("Напомню о работе", kContextBudget));
    EXPECT_TRUE(repeated.profileFactIdentifiers.empty());
    EXPECT_EQ(repeated.text, "Поздравляю!");
    EXPECT_EQ(m_profile.profile().facts.size(), 1U);
}

TEST_F(ConversationServiceTest, FactAndMoodEntryShareOriginalMessageAndDiaryReference)
{
    m_client.setResult(LlmResponse{.reply = "Спасибо", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::ExplicitRequest},
        .profileFacts = {{.text = "Начала работать разработчиком"}}});
    auto service = conversation();
    const std::string original = "Запиши настроение: рада, устроилась разработчиком\n  Подробности";
    const auto reply = std::get<ConversationReply>(service.sendMessage(original, kContextBudget));
    ASSERT_TRUE(reply.moodIdentifier);
    ASSERT_EQ(diary().size(), 1U);
    EXPECT_EQ(diary().front().note, original);
    const auto fact = m_profile.profile().facts.front();
    ASSERT_TRUE(fact.source);
    EXPECT_EQ(fact.source->moodIdentifier, reply.moodIdentifier);
    EXPECT_EQ(fact.source->messageIdentifier, m_chat.loadMessages().front().identifier);
    EXPECT_NE(reply.text.find("Настроение сохранено в дневник."), std::string::npos);
    EXPECT_NE(reply.text.find("Сохранено в памяти:"), std::string::npos);
}

TEST_F(ConversationServiceTest, RevisionUsesVisibleStableIdAndNextContextContainsOnlyCurrentVersion)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Работаю в A", {"работа"}, true)));
    m_client.setResult(LlmResponse{.reply = "Понял", .profileFacts = {
        {.text = "Работаю в B", .replacesFactIdentifier = 1}}});
    auto service = conversation();
    const auto result = service.sendMessage("Теперь работа в B", kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
    EXPECT_EQ(m_client.requests().front().profileFactIdentifiers, (std::vector<std::int64_t>{1}));
    EXPECT_NE(m_client.requests().front().messages.front().text.find("[Факт ID=1]\nРаботаю в A"), std::string::npos);
    const auto snapshot = m_profile.profile();
    ASSERT_EQ(snapshot.facts.size(), 2U);
    EXPECT_EQ(snapshot.facts.front().supersededByFactIdentifier, 2);
    EXPECT_EQ(snapshot.facts.back().replacesFactIdentifier, 1);
    ASSERT_TRUE(snapshot.facts.back().source);
    EXPECT_EQ(snapshot.facts.back().source->messageIdentifier, m_chat.loadMessages().front().identifier);
    m_client.setResult(LlmResponse{.reply = "Ответ"});
    static_cast<void>(service.sendMessage("Работа", kContextBudget));
    EXPECT_EQ(m_client.requests().back().profileFactIdentifiers, (std::vector<std::int64_t>{2}));
    const auto& system = m_client.requests().back().messages.front().text;
    EXPECT_EQ(system.find("[Факт ID=1]"), std::string::npos);
    EXPECT_NE(system.find("[Факт ID=2]\nРаботаю в B"), std::string::npos);
    EXPECT_EQ(m_profile.profile().facts, snapshot.facts);
}

TEST_F(ConversationServiceTest, FactOmittedByBudgetCannotBeRevisedAndResponseHasNoSideEffects)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Работаю в A", {}, true)));
    m_client.setResult(LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::ExplicitRequest},
        .profileFacts = {{.text = "Работаю в B", .replacesFactIdentifier = 1}}});
    const MentorPrompt prompt;
    const std::string question = "Исправление";
    const auto mandatoryBudget = text::charCount(prompt.build(false)) + text::charCount(question);
    auto service = conversation();
    const auto result = service.sendMessage(question, mandatoryBudget);
    ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
    EXPECT_EQ(std::get<LlmFailure>(result).kind, LlmFailureKind::InvalidResponse);
    ASSERT_EQ(m_client.requests().size(), 1U);
    EXPECT_TRUE(m_client.requests().front().profileFactIdentifiers.empty());
    EXPECT_EQ(m_chat.loadMessages().size(), 1U);
    EXPECT_TRUE(diary().empty());
    EXPECT_EQ(m_profile.profile().facts.size(), 1U);
    EXPECT_FALSE(m_profile.profile().facts.front().supersededByFactIdentifier);
}

TEST_F(ConversationServiceTest, AmbiguousProfileRevisionIsRejectedBeforeMoodRecording)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Работаю в A", {}, true)));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Работаю в B", {}, true)));
    const auto before = m_profile.profile();
    m_client.setResult(LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::ExplicitRequest},
        .profileFacts = {{.text = "Работаю в B", .replacesFactIdentifier = 1}}});
    auto service = conversation();
    const auto result = service.sendMessage("Исправь работу и запиши настроение", kContextBudget);
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(result));
    EXPECT_TRUE(diary().empty());
    EXPECT_EQ(m_profile.profile().facts, before.facts);
    EXPECT_EQ(m_chat.loadMessages().size(), 1U);
}

/// @brief Модель, заканчивающая ответ уже в следующие местные сутки.
class MidnightLlmClient final : public LlmClient {
public:
    explicit MidnightLlmClient(FakeClock& clock);

protected:
    [[nodiscard]] LlmResult requestCompletion(const LlmRequest& request) override;

private:
    FakeClock& m_clock;
};

MidnightLlmClient::MidnightLlmClient(FakeClock& clock)
    : m_clock{clock}
{
}

LlmResult MidnightLlmClient::requestCompletion([[maybe_unused]] const LlmRequest& request)
{
    m_clock.advance(std::chrono::hours{4});

    return LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply}};
}

TEST_F(ConversationServiceTest, ExpiryDuringLlmRequestPreventsLateEveningRecording)
{
    MidnightLlmClient client{m_clock};
    ConversationService service{m_clock, m_chat, m_profile, m_daily, client,
        m_moodService, m_checkIn, m_checkIns, m_transactions};
    ASSERT_TRUE(service.offerEveningCheckIn());
    const auto result = service.sendMessage("Мне хорошо", kContextBudget);
    EXPECT_TRUE(std::holds_alternative<ConversationRejection>(result));
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_TRUE(diary().empty());
    EXPECT_EQ(m_chat.loadMessages().size(), 2U);
    EXPECT_EQ(m_chat.loadMessages().back().role, MessageRole::User);
}

TEST_F(ConversationServiceTest, CommitFailureRollsBackAllWritesAndRetryDoesNotDuplicateUser)
{
    auto service = conversation();
    const auto promptIdentifier = service.offerEveningCheckIn();
    ASSERT_TRUE(promptIdentifier);
    m_client.setResult(LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::EveningCheckInReply},
        .profileFacts = {{.text = "Важный факт"}}});
    m_transactions.failNextCommit();
    EXPECT_THROW(static_cast<void>(service.sendMessage("Исходное сообщение", kContextBudget)), std::runtime_error);
    EXPECT_FALSE(m_transactions.isActive());
    EXPECT_TRUE(diary().empty());
    EXPECT_TRUE(m_profile.profile().facts.empty());
    ASSERT_TRUE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_checkIns.pendingCheckIn()->messageIdentifier, promptIdentifier);
    ASSERT_EQ(m_chat.loadMessages().size(), 2U);
    const auto userIdentifier = m_chat.loadMessages().back().identifier;
    const auto result = service.retryLastMessage(kContextBudget);
    ASSERT_TRUE(std::holds_alternative<ConversationReply>(result));
    EXPECT_EQ(diary().size(), 1U);
    EXPECT_EQ(m_profile.profile().facts.size(), 1U);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    ASSERT_EQ(m_chat.loadMessages().size(), 3U);
    EXPECT_EQ(m_chat.loadMessages()[1].identifier, userIdentifier);
}

}  // namespace
}  // namespace msa::core

