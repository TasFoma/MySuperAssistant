#include "FakeClock.h"
#include "FakeLlmClient.h"
#include "InMemoryMoodRepository.h"

#include "msa/core/LlmContextBuilder.h"
#include "msa/core/MoodService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

[[nodiscard]] LlmRequest validRequest()
{
    return LlmRequest{.messages = {
        {.role = LlmRole::System, .text = "Инструкции"},
        {.role = LlmRole::User, .text = "  Запиши: сегодня радостно\n  Важные подробности 🙂  "}}};
}

TEST(LlmClientTest, AcceptsBuilderMessagesAndPreservesFullRequestAndReply)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{.systemPrompt = "Инструкции",
        .currentQuestion = "  Вопрос\n🙂  ", .selectedFacts = {"Полный факт"},
        .history = {{.identifier = 1, .role = MessageRole::User, .text = "Прошлый вопрос"},
            {.identifier = 2, .role = MessageRole::Assistant, .text = "Прошлый ответ"}}};
    const auto context = std::get<LlmContext>(builder.build(input, 1000));
    const std::string reply = "  Полный ответ\n  с пробелами 🙂  ";
    FakeLlmClient fake{LlmResponse{.reply = reply}};
    LlmClient& client = fake;
    LlmRequest request{.messages = context.messages};
    const auto result = client.complete(request);
    ASSERT_TRUE(std::holds_alternative<LlmResponse>(result));
    EXPECT_EQ(std::get<LlmResponse>(result).reply, reply);
    EXPECT_FALSE(std::get<LlmResponse>(result).tone);
    EXPECT_FALSE(std::get<LlmResponse>(result).moodRecording);
    ASSERT_EQ(fake.requests().size(), 1U);
    ASSERT_EQ(fake.requests().front().messages.size(), context.messages.size());
    for (std::size_t index = 0; index < context.messages.size(); ++index) {
        EXPECT_EQ(fake.requests().front().messages[index].text, context.messages[index].text);
        EXPECT_EQ(fake.requests().front().messages[index].role, context.messages[index].role);
    }
    request.messages.back().text.clear();
    EXPECT_EQ(fake.requests().front().messages.back().text, input.currentQuestion);
}

TEST(LlmClientTest, RejectsInvalidRequestsWithoutCallingImplementation)
{
    FakeLlmClient client{LlmResponse{.reply = "Ответ"}};
    std::vector<LlmRequest> requests{{},
        {.messages = {{.role = LlmRole::User, .text = "Вопрос"}}}};
    auto request = validRequest();
    request.messages.front().role = LlmRole::Assistant;
    requests.push_back(request);
    request = validRequest();
    request.messages.back().role = LlmRole::Assistant;
    requests.push_back(request);
    for (const auto& blank : std::vector<std::string>{"", " \t\n", "\xC2\xA0\xE3\x80\x80"}) {
        request = validRequest();
        request.messages.front().text = blank;
        requests.push_back(request);
        request = validRequest();
        request.messages.back().text = blank;
        requests.push_back(request);
    }
    for (const auto role : {LlmRole::System, static_cast<LlmRole>(99)}) {
        request = validRequest();
        request.messages.insert(request.messages.begin() + 1, {.role = role, .text = "История"});
        requests.push_back(request);
    }
    request = validRequest();
    request.messages.insert(request.messages.begin() + 1, {.role = LlmRole::User, .text = "\t"});
    requests.push_back(request);
    for (const auto& invalid : requests) {
        const auto result = client.complete(invalid);
        ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
        EXPECT_EQ(std::get<LlmFailure>(result).kind, LlmFailureKind::InvalidRequest);
        EXPECT_FALSE(std::get<LlmFailure>(result).reason.empty());
    }
    EXPECT_TRUE(client.requests().empty());
}

TEST(LlmClientTest, ToneAloneNeverBecomesMoodRecording)
{
    for (const auto score : {MoodScore::Negative, MoodScore::Neutral, MoodScore::Positive}) {
        FakeLlmClient client{LlmResponse{.reply = "Поддержка", .tone = score}};
        const auto response = std::get<LlmResponse>(client.complete(validRequest()));
        EXPECT_EQ(response.tone, score);
        EXPECT_FALSE(response.moodRecording);
        EXPECT_EQ(client.requests().size(), 1U);
    }
}

TEST(LlmClientTest, ExplicitRecordingUsesOriginalNoteAndIndependentScore)
{
    const auto request = validRequest();
    for (const auto score : {MoodScore::Negative, MoodScore::Neutral, MoodScore::Positive}) {
        FakeLlmClient client{LlmResponse{.reply = "Ответ", .tone = MoodScore::Negative,
            .moodRecording = MoodRecordingProposal{.score = score,
                .reason = MoodRecordingReason::ExplicitRequest, .note = "Сокращение от модели"}}};
        const auto response = std::get<LlmResponse>(client.complete(request));
        ASSERT_TRUE(response.moodRecording);
        EXPECT_EQ(response.moodRecording->score, score);
        EXPECT_EQ(response.moodRecording->reason, MoodRecordingReason::ExplicitRequest);
        EXPECT_EQ(response.moodRecording->note, request.messages.back().text);
        EXPECT_EQ(response.tone, MoodScore::Negative);
        EXPECT_FALSE(client.requests().front().awaitingEveningCheckIn);
    }
}

TEST(LlmClientTest, EveningCheckInReplyRequiresWaitingStateAndPreservesNote)
{
    FakeLlmClient client{LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Neutral, .reason = MoodRecordingReason::EveningCheckInReply}}};
    auto request = validRequest();
    const auto rejected = client.complete(request);
    ASSERT_TRUE(std::holds_alternative<LlmFailure>(rejected));
    EXPECT_EQ(std::get<LlmFailure>(rejected).kind, LlmFailureKind::InvalidResponse);
    request.awaitingEveningCheckIn = true;
    const auto accepted = client.complete(request);
    ASSERT_TRUE(std::holds_alternative<LlmResponse>(accepted));
    const auto proposal = std::get<LlmResponse>(accepted).moodRecording;
    ASSERT_TRUE(proposal);
    EXPECT_EQ(proposal->note, request.messages.back().text);
    EXPECT_TRUE(client.requests().back().awaitingEveningCheckIn);
    EXPECT_EQ(client.requests().size(), 2U);
    FakeLlmClient unrelatedReply{LlmResponse{.reply = "О другом"}};
    EXPECT_FALSE(std::get<LlmResponse>(unrelatedReply.complete(request)).moodRecording);
}

TEST(LlmClientTest, RejectsInvalidFullResponseWithoutRetryOrPartialReply)
{
    const auto unknownScore = static_cast<MoodScore>(99);
    const std::vector<LlmResponse> invalidResponses{
        {.reply = ""}, {.reply = " \t\xC2\xA0\xE3\x80\x80"},
        {.reply = "Ответ", .tone = unknownScore},
        {.reply = "Ответ", .moodRecording = MoodRecordingProposal{.score = unknownScore}},
        {.reply = "Ответ", .moodRecording = MoodRecordingProposal{
            .reason = static_cast<MoodRecordingReason>(99)}}};
    for (const auto& response : invalidResponses) {
        FakeLlmClient client{response};
        const auto result = client.complete(validRequest());
        ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
        EXPECT_EQ(std::get<LlmFailure>(result).kind, LlmFailureKind::InvalidResponse);
        EXPECT_FALSE(std::get<LlmFailure>(result).reason.empty());
        EXPECT_EQ(client.requests().size(), 1U);
    }
}

TEST(LlmClientTest, ReturnsTransportAndFormatFailuresWithoutRetry)
{
    for (const auto kind : {LlmFailureKind::Unavailable, LlmFailureKind::InvalidResponse}) {
        FakeLlmClient client{LlmFailure{.kind = kind, .reason = "Понятная ошибка"}};
        const auto result = client.complete(validRequest());
        ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
        EXPECT_EQ(std::get<LlmFailure>(result).kind, kind);
        EXPECT_EQ(std::get<LlmFailure>(result).reason, "Понятная ошибка");
        EXPECT_EQ(client.requests().size(), 1U);
    }
    FakeLlmClient client{LlmFailure{.reason = " \n"}};
    const auto result = client.complete(validRequest());
    ASSERT_TRUE(std::holds_alternative<LlmFailure>(result));
    EXPECT_FALSE(std::get<LlmFailure>(result).reason.empty());
    EXPECT_EQ(client.requests().size(), 1U);
}

TEST(LlmClientTest, CallerCanSaveOriginalProposalThroughMoodService)
{
    FakeClock clock{std::chrono::local_days{std::chrono::year{2026} / 11 / 6}};
    InMemoryMoodRepository repository;
    MoodService moods{clock, repository};
    FakeLlmClient client{LlmResponse{.reply = "Ответ", .moodRecording = MoodRecordingProposal{
        .score = MoodScore::Positive, .reason = MoodRecordingReason::ExplicitRequest}}};
    const auto request = validRequest();
    const auto response = std::get<LlmResponse>(client.complete(request));
    EXPECT_TRUE(std::get<std::vector<MoodEntry>>(moods.moodHistory()).empty());
    ASSERT_TRUE(response.moodRecording);
    const auto& proposal = *response.moodRecording;
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(moods.logMood(proposal.score, proposal.note)));
    const auto history = std::get<std::vector<MoodEntry>>(moods.moodHistory());
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history.front().note, request.messages.back().text);
    EXPECT_EQ(history.front().score, MoodScore::Positive);
    EXPECT_EQ(history.front().recordedAt, clock.now());
}

}  // namespace
}  // namespace msa::core

