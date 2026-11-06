#include "FakeClock.h"
#include "InMemoryChatRepository.h"
#include "InMemoryEventRepository.h"
#include "InMemoryHabitRepository.h"
#include "InMemoryTaskRepository.h"
#include "InMemoryUserProfileRepository.h"

#include "msa/core/ChatService.h"
#include "msa/core/DailyContextService.h"
#include "msa/core/LlmContextBuilder.h"
#include "msa/core/UserProfileService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

TEST(LlmContextBuilderTest, MandatoryTextsFitExactlyByUnicodeCodePoints)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{.systemPrompt = "Я🙂", .currentQuestion = "е\u0301"};
    const auto context = std::get<LlmContext>(builder.build(input, 4));
    ASSERT_EQ(context.messages.size(), 2U);
    EXPECT_EQ(context.messages[0].role, LlmRole::System);
    EXPECT_EQ(context.messages[0].text, input.systemPrompt);
    EXPECT_EQ(context.messages[1].role, LlmRole::User);
    EXPECT_EQ(context.messages[1].text, input.currentQuestion);
    EXPECT_EQ(context.usedChars, 4U);
    EXPECT_EQ(context.omittedFacts, 0U);
    EXPECT_EQ(context.omittedHistoryMessages, 0U);
    EXPECT_FALSE(context.dailyContextIncluded);
    for (const auto budget : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
        const auto rejected = builder.build(input, budget);
        ASSERT_TRUE(std::holds_alternative<LlmContextRejection>(rejected));
        EXPECT_FALSE(std::get<LlmContextRejection>(rejected).reason.empty());
    }
}

TEST(LlmContextBuilderTest, IncludesSectionsAndRolesAndCountsFormattingInTotalBudget)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{
        .systemPrompt = "S", .currentQuestion = "В", .selectedFacts = {"А"}, .dailyContext = "Д",
        .history = {ChatMessage{.identifier = 2, .chatIdentifier = 1,
                       .role = MessageRole::Assistant, .text = "О"},
                    ChatMessage{.identifier = 1, .chatIdentifier = 1, .text = "У"}}};
    const auto context = std::get<LlmContext>(builder.build(input, 39));
    ASSERT_EQ(context.messages.size(), 4U);
    EXPECT_EQ(context.messages[0].text, "S\n\nФакты профиля:\n• А\n\nСводка дня:\nД");
    EXPECT_EQ(context.messages[1].role, LlmRole::User);
    EXPECT_EQ(context.messages[1].text, "У");
    EXPECT_EQ(context.messages[2].role, LlmRole::Assistant);
    EXPECT_EQ(context.messages[2].text, "О");
    EXPECT_EQ(context.messages[3].text, "В");
    EXPECT_EQ(context.usedChars, 39U);
    EXPECT_TRUE(context.dailyContextIncluded);
    EXPECT_EQ(context.omittedFacts, 0U);
    EXPECT_EQ(context.omittedHistoryMessages, 0U);
}

TEST(LlmContextBuilderTest, FactsTakePriorityAndOmittedSectionsDoNotLeaveHeadings)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{
        .systemPrompt = "S", .currentQuestion = "В", .selectedFacts = {"А", std::string(100, 'x')},
        .dailyContext = "Д",
        .history = {ChatMessage{.identifier = 1, .text = "У"},
                    ChatMessage{.identifier = 2, .role = MessageRole::Assistant, .text = "О"}}};
    const auto context = std::get<LlmContext>(builder.build(input, 23));
    ASSERT_EQ(context.messages.size(), 3U);
    EXPECT_EQ(context.messages[0].text, "S\n\nФакты профиля:\n• А");
    EXPECT_EQ(context.messages[1].text, "О");
    EXPECT_EQ(context.messages.back().text, "В");
    EXPECT_EQ(context.usedChars, 23U);
    EXPECT_EQ(context.omittedFacts, 1U);
    EXPECT_FALSE(context.dailyContextIncluded);
    EXPECT_EQ(context.omittedHistoryMessages, 1U);
}

TEST(LlmContextBuilderTest, FactsRemainPriorityPrefixAndDailySummaryIsIncludedOnlyWhole)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{
        .systemPrompt = "S", .currentQuestion = "В", .selectedFacts = {std::string(100, 'x'), "А"},
        .dailyContext = "Д"};
    const auto omitted = std::get<LlmContext>(builder.build(input, 16));
    EXPECT_EQ(omitted.messages.front().text, "S");
    EXPECT_EQ(omitted.usedChars, 2U);
    EXPECT_EQ(omitted.omittedFacts, 2U);
    EXPECT_FALSE(omitted.dailyContextIncluded);
    const auto included = std::get<LlmContext>(builder.build(input, 17));
    EXPECT_EQ(included.messages.front().text, "S\n\nСводка дня:\nД");
    EXPECT_EQ(included.usedChars, 17U);
    EXPECT_TRUE(included.dailyContextIncluded);
    EXPECT_EQ(included.omittedFacts, 2U);
}

TEST(LlmContextBuilderTest, HistoryIsContinuousSuffixAndTooLargeLatestMessageIsOptional)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{
        .systemPrompt = "S", .currentQuestion = "В",
        .history = {ChatMessage{.identifier = 1, .text = "а"},
                    ChatMessage{.identifier = 2, .text = "12345678"},
                    ChatMessage{.identifier = 3, .text = "Да"},
                    ChatMessage{.identifier = 4, .role = MessageRole::Assistant, .text = "🙂"}}};
    const auto context = std::get<LlmContext>(builder.build(input, 6));
    ASSERT_EQ(context.messages.size(), 4U);
    EXPECT_EQ(context.messages[1].text, "Да");
    EXPECT_EQ(context.messages[2].text, "🙂");
    EXPECT_EQ(context.usedChars, 5U);
    EXPECT_EQ(context.omittedHistoryMessages, 2U);
    const auto mandatoryOnly = std::get<LlmContext>(builder.build(input, 2));
    ASSERT_EQ(mandatoryOnly.messages.size(), 2U);
    EXPECT_EQ(mandatoryOnly.omittedHistoryMessages, 4U);
    EXPECT_EQ(mandatoryOnly.messages.back().text, "В");
}

TEST(LlmContextBuilderTest, SavedCurrentQuestionAppearsOnceWithoutRemovingOlderIdenticalQuestion)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{
        .systemPrompt = "S", .currentQuestion = "Да",
        .history = {ChatMessage{.identifier = 1, .text = "Да"},
                    ChatMessage{.identifier = 2, .role = MessageRole::Assistant, .text = "Уточни"},
                    ChatMessage{.identifier = 3, .text = "Да"},
                    ChatMessage{.identifier = 4, .role = MessageRole::Assistant, .text = "Поздний ответ"}},
        .currentMessageId = 3};
    const auto context = std::get<LlmContext>(builder.build(input, 100));
    ASSERT_EQ(context.messages.size(), 4U);
    EXPECT_EQ(context.messages[1].text, "Да");
    EXPECT_EQ(context.messages[2].text, "Уточни");
    EXPECT_EQ(context.messages[3].text, "Да");
    EXPECT_EQ(context.messages[3].role, LlmRole::User);
    EXPECT_EQ(context.omittedHistoryMessages, 0U);
    EXPECT_EQ(input.history.size(), 4U);
    EXPECT_EQ(input.history[3].text, "Поздний ответ");
}

TEST(LlmContextBuilderTest, RejectsBlankRequiredDataAndInvalidFacts)
{
    const LlmContextBuilder builder;
    for (const auto& blank : {std::string{}, std::string{" \t\n\u00a0"}}) {
        const LlmContextInput prompt{.systemPrompt = blank, .currentQuestion = "В"};
        const LlmContextInput question{.systemPrompt = "S", .currentQuestion = blank};
        const LlmContextInput fact{.systemPrompt = "S", .currentQuestion = "В", .selectedFacts = {blank}};
        for (const auto& input : {prompt, question, fact}) {
            const auto result = builder.build(input, 100);
            ASSERT_TRUE(std::holds_alternative<LlmContextRejection>(result));
            EXPECT_FALSE(std::get<LlmContextRejection>(result).reason.empty());
        }
    }
}

TEST(LlmContextBuilderTest, RejectsMixedChatsDuplicateIdentifiersAndUnknownRoles)
{
    const LlmContextBuilder builder;
    for (const auto& history : std::vector<std::vector<ChatMessage>>{
        {ChatMessage{.identifier = 1, .chatIdentifier = 1}, ChatMessage{.identifier = 2, .chatIdentifier = 2}},
        {ChatMessage{.identifier = 1}, ChatMessage{.identifier = 1}},
        {ChatMessage{.identifier = 0}},
        {ChatMessage{.identifier = 1, .role = static_cast<MessageRole>(255)}}}) {
        const LlmContextInput input{.systemPrompt = "S", .currentQuestion = "В", .history = history};
        EXPECT_TRUE(std::holds_alternative<LlmContextRejection>(builder.build(input, 100)));
    }
}

TEST(LlmContextBuilderTest, RejectsMismatchedSavedCurrentQuestion)
{
    const LlmContextBuilder builder;
    for (const auto& message : {
        ChatMessage{.identifier = 2, .text = "В"},
        ChatMessage{.identifier = 1, .text = "Другой вопрос"},
        ChatMessage{.identifier = 1, .role = MessageRole::Assistant, .text = "В"}}) {
        const LlmContextInput input{.systemPrompt = "S", .currentQuestion = "В",
            .history = {message}, .currentMessageId = 1};
        EXPECT_TRUE(std::holds_alternative<LlmContextRejection>(builder.build(input, 100)));
    }
}

TEST(LlmContextBuilderTest, CombinesRealServiceSnapshotsWithoutWritingOrTruncatingSources)
{
    FakeClock clock{std::chrono::local_days{std::chrono::year{2026} / 11 / 6}};
    InMemoryChatRepository chats;
    ChatService chat{clock, chats};
    static_cast<void>(chat.addMessage("Предыдущий вопрос", MessageRole::User));
    static_cast<void>(chat.addMessage("Предыдущий ответ", MessageRole::Assistant));
    const std::string question = "  Текущий вопрос\n🙂  ";
    const auto current = std::get<std::int64_t>(chat.addMessage(question, MessageRole::User));
    InMemoryUserProfileRepository profiles;
    UserProfileService profile{profiles};
    static_cast<void>(profile.rememberFact("Важное событие\n" + std::string(200, 'x')));
    InMemoryEventRepository events;
    InMemoryTaskRepository tasks;
    InMemoryHabitRepository habits;
    CalendarService calendar{events};
    TaskService taskService{clock, tasks};
    HabitService habitService{clock, habits};
    DailyContextService daily{clock, calendar, taskService, habitService};
    const LlmContextInput input{.systemPrompt = "  Инструкции\n", .currentQuestion = question,
        .selectedFacts = {profile.profile().facts.front().text}, .dailyContext = daily.dailyContext(),
        .history = chat.loadMessages(), .currentMessageId = current};
    const LlmContextBuilder builder;
    const auto context = std::get<LlmContext>(builder.build(input, std::numeric_limits<std::size_t>::max()));
    ASSERT_EQ(context.messages.size(), 4U);
    EXPECT_TRUE(context.messages.front().text.starts_with(input.systemPrompt));
    EXPECT_NE(context.messages.front().text.find(input.selectedFacts.front()), std::string::npos);
    EXPECT_NE(context.messages.front().text.find(input.dailyContext), std::string::npos);
    EXPECT_EQ(context.messages.back().text, question);
    EXPECT_EQ(profile.profile().facts.front().text, input.selectedFacts.front());
    EXPECT_EQ(chat.loadMessages().size(), 3U);
    EXPECT_EQ(chat.loadMessages().back().identifier, current);
    EXPECT_EQ(chat.loadMessages().back().text, question);
    EXPECT_EQ(daily.dailyContext(), input.dailyContext);
    const auto repeated = std::get<LlmContext>(builder.build(input, std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(repeated.usedChars, context.usedChars);
    EXPECT_EQ(repeated.messages.front().text, context.messages.front().text);
}

}  // namespace
}  // namespace msa::core

