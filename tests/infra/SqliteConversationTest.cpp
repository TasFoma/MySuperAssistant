#include "../core/FakeClock.h"
#include "../core/FakeLlmClient.h"

#include "msa/core/CalendarService.h"
#include "msa/core/ConversationService.h"
#include "msa/core/DailyContextService.h"
#include "msa/core/HabitService.h"
#include "msa/core/TaskService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteEveningCheckInRepository.h"
#include "msa/infra/SqliteEventRepository.h"
#include "msa/infra/SqliteGreetingRepository.h"
#include "msa/infra/SqliteHabitRepository.h"
#include "msa/infra/SqliteMoodRepository.h"
#include "msa/infra/SqliteSchema.h"
#include "msa/infra/SqliteTaskRepository.h"
#include "msa/infra/SqliteUserProfileRepository.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::infra {
namespace {

constexpr std::size_t kContextBudget = 20000;
constexpr auto kDate = std::chrono::year{2026} / 11 / 6;

[[nodiscard]] core::LlmResponse fullResponse()
{
    return {.reply = "Поддерживающий ответ", .moodRecording = core::MoodRecordingProposal{
        .score = core::MoodScore::Positive, .reason = core::MoodRecordingReason::EveningCheckInReply},
        .profileFacts = {{.text = "Важный факт"}}};
}

class SqliteConversationTest : public ::testing::Test {
protected:
    SqliteConversationTest();
    [[nodiscard]] core::ConversationService conversation();

    QTemporaryDir m_directory{QStringLiteral("msa-conversation-test-XXXXXX")};
    SqliteDatabase m_database;
    core::FakeClock m_clock;
    SqliteChatRepository m_chats;
    core::ChatService m_chat;
    SqliteUserProfileRepository m_profiles;
    core::UserProfileService m_profile;
    SqliteEventRepository m_events;
    core::CalendarService m_calendar;
    SqliteTaskRepository m_tasks;
    core::TaskService m_taskService;
    SqliteHabitRepository m_habits;
    core::HabitService m_habitService;
    core::DailyContextService m_daily;
    core::FakeLlmClient m_client;
    SqliteMoodRepository m_moods;
    core::MoodService m_moodService;
    SqliteEveningCheckInRepository m_checkIns;
    core::EveningCheckInService m_checkIn;
};

SqliteConversationTest::SqliteConversationTest()
    : m_database{m_directory.filePath(QStringLiteral("чат.sqlite")), sqliteMigrations()},
      m_clock{std::chrono::local_days{kDate} + std::chrono::hours{20}},
      m_chats{m_database}, m_chat{m_clock, m_chats}, m_profiles{m_database}, m_profile{m_profiles},
      m_events{m_database}, m_calendar{m_events}, m_tasks{m_database}, m_taskService{m_clock, m_tasks},
      m_habits{m_database}, m_habitService{m_clock, m_habits},
      m_daily{m_clock, m_calendar, m_taskService, m_habitService}, m_client{fullResponse()},
      m_moods{m_database}, m_moodService{m_clock, m_moods}, m_checkIns{m_database}, m_checkIn{m_clock, m_checkIns}
{
}

core::ConversationService SqliteConversationTest::conversation()
{
    return core::ConversationService{m_clock, m_chat, m_profile, m_daily, m_client,
        m_moodService, m_checkIn, m_checkIns, m_database};
}

TEST_F(SqliteConversationTest, CommitsDiaryProfileAssistantAndClearingTogetherAndRestoresFromFile)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    const std::string original = "  Полный исходник\nмне хорошо 🙂  ";
    const auto result = service.sendMessage(original, kContextBudget);
    ASSERT_TRUE(std::holds_alternative<core::ConversationReply>(result));
    const auto& reply = std::get<core::ConversationReply>(result);
    ASSERT_TRUE(reply.moodIdentifier);
    ASSERT_EQ(reply.profileFactIdentifiers.size(), 1U);
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_EQ(m_checkIns.lastDate(), kDate);
    SqliteDatabase reopened{m_directory.filePath(QStringLiteral("чат.sqlite")), sqliteMigrations()};
    SqliteChatRepository chats{reopened};
    core::ChatService restoredChat{m_clock, chats};
    const auto history = restoredChat.loadMessages();
    ASSERT_EQ(history.size(), 3U);
    EXPECT_EQ(history[1].text, original);
    EXPECT_EQ(history.back().identifier, reply.messageIdentifier);
    EXPECT_EQ(history.back().text, reply.text);
    EXPECT_NE(reply.text.find("Настроение сохранено"), std::string::npos);
    EXPECT_NE(reply.text.find("Сохранено в памяти"), std::string::npos);
    SqliteMoodRepository moods{reopened};
    ASSERT_EQ(moods.all().size(), 1U);
    EXPECT_EQ(moods.all().front().note, original);
    SqliteUserProfileRepository profiles{reopened};
    const auto facts = profiles.load().facts;
    ASSERT_EQ(facts.size(), 1U);
    ASSERT_TRUE(facts.front().source);
    EXPECT_EQ(facts.front().source->messageIdentifier, history[1].identifier);
    EXPECT_EQ(facts.front().source->moodIdentifier, reply.moodIdentifier);
    SqliteEveningCheckInRepository checkIns{reopened};
    EXPECT_FALSE(checkIns.pendingCheckIn());
    EXPECT_TRUE(std::holds_alternative<core::ConversationRejection>(service.retryLastMessage(kContextBudget)));
    EXPECT_EQ(m_client.requests().size(), 1U);
}

TEST_F(SqliteConversationTest, EveryWriteFailureRollsBackWholeReplyAndExplicitRetryKeepsSingleUser)
{
    struct Failure {
        const char* name;
        const char* trigger;
    };
    const std::array failures{
        Failure{"дневник", "CREATE TRIGGER fail_reply BEFORE INSERT ON mood_entries "
            "BEGIN SELECT RAISE(ABORT, 'diary failure'); END"},
        Failure{"профиль", "CREATE TRIGGER fail_reply BEFORE INSERT ON profile_facts "
            "BEGIN SELECT RAISE(ABORT, 'profile failure'); END"},
        Failure{"Assistant", "CREATE TRIGGER fail_reply BEFORE INSERT ON chat_messages WHEN NEW.role = 'Assistant' "
            "BEGIN SELECT RAISE(ABORT, 'assistant failure'); END"},
        Failure{"ожидание", "CREATE TRIGGER fail_reply BEFORE DELETE ON pending_evening_check_in "
            "BEGIN SELECT RAISE(ABORT, 'pending failure'); END"}};
    for (const auto& failure : failures) {
        SCOPED_TRACE(failure.name);
        auto service = conversation();
        // Каждый случай использует своё ожидание; дата выдачи не участвует в записи ответа.
        m_clock.advance(std::chrono::days{1});
        const auto prompt = service.offerEveningCheckIn();
        ASSERT_TRUE(prompt);
        const auto profileBefore = m_profiles.load();
        const auto moodCount = m_moods.all().size();
        const auto historyCount = m_chat.loadMessages().size();
        m_client.setResult(core::LlmResponse{.reply = "Ответ", .moodRecording = fullResponse().moodRecording,
            .profileFacts = {{.text = std::string{"Факт для "} + failure.name}}});
        m_database.execute(QString::fromUtf8(failure.trigger));
        EXPECT_THROW(static_cast<void>(service.sendMessage("Исходное сообщение", kContextBudget)), std::runtime_error);
        EXPECT_EQ(m_moods.all().size(), moodCount);
        EXPECT_EQ(m_profiles.load().facts, profileBefore.facts);
        const auto pending = m_checkIns.pendingCheckIn();
        ASSERT_TRUE(pending);
        EXPECT_EQ(pending->messageIdentifier, prompt);
        const auto history = m_chat.loadMessages();
        ASSERT_EQ(history.size(), historyCount + 1);
        EXPECT_EQ(history.back().role, core::MessageRole::User);
        const auto userIdentifier = history.back().identifier;
        m_database.execute(QStringLiteral("DROP TRIGGER fail_reply"));
        const auto retry = service.retryLastMessage(kContextBudget);
        ASSERT_TRUE(std::holds_alternative<core::ConversationReply>(retry));
        EXPECT_EQ(m_moods.all().size(), moodCount + 1);
        EXPECT_EQ(m_profiles.load().facts.size(), profileBefore.facts.size() + 1);
        EXPECT_FALSE(m_checkIns.pendingCheckIn());
        const auto afterRetry = m_chat.loadMessages();
        ASSERT_EQ(afterRetry.size(), historyCount + 2);
        EXPECT_EQ(afterRetry[afterRetry.size() - 2].identifier, userIdentifier);
    }
}

TEST_F(SqliteConversationTest, DeferredCommitFailureRollsBackReplyAndKeepsUser)
{
    auto service = conversation();
    ASSERT_TRUE(service.offerEveningCheckIn());
    m_database.execute(QStringLiteral("CREATE TABLE commit_probe ("
        "identifier INTEGER REFERENCES chats(identifier) DEFERRABLE INITIALLY DEFERRED)"));
    m_database.execute(QStringLiteral("CREATE TRIGGER fail_commit AFTER INSERT ON mood_entries "
        "BEGIN INSERT INTO commit_probe VALUES (999999); END"));
    EXPECT_THROW(static_cast<void>(service.sendMessage("Хорошо", kContextBudget)), std::runtime_error);
    EXPECT_TRUE(m_moods.all().empty());
    EXPECT_TRUE(m_profiles.load().facts.empty());
    EXPECT_TRUE(m_checkIns.pendingCheckIn());
    ASSERT_EQ(m_chat.loadMessages().size(), 2U);
    EXPECT_EQ(m_chat.loadMessages().back().role, core::MessageRole::User);
    m_database.execute(QStringLiteral("DROP TRIGGER fail_commit"));
    EXPECT_TRUE(std::holds_alternative<core::ConversationReply>(service.retryLastMessage(kContextBudget)));
    EXPECT_EQ(m_moods.all().size(), 1U);
}

TEST_F(SqliteConversationTest, RevisionFailureRestoresPreviousFactAndItsTagsBeforeRetry)
{
    ASSERT_TRUE(std::holds_alternative<std::monostate>(m_profile.rememberFact("Старая работа", {"работа"}, true)));
    const auto oldProfile = m_profiles.load();
    const auto oldIdentifier = oldProfile.facts.front().identifier;
    m_client.setResult(core::LlmResponse{.reply = "Ответ", .profileFacts = {
        {.text = "Новая работа", .replacesFactIdentifier = oldIdentifier}}});
    auto service = conversation();
    m_database.execute(QStringLiteral("CREATE TRIGGER fail_assistant BEFORE INSERT ON chat_messages "
        "WHEN NEW.role = 'Assistant' BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    EXPECT_THROW(static_cast<void>(service.sendMessage("Теперь новая работа", kContextBudget)), std::runtime_error);
    EXPECT_EQ(m_profiles.load().facts, oldProfile.facts);
    m_database.execute(QStringLiteral("DROP TRIGGER fail_assistant"));
    const auto result = service.retryLastMessage(kContextBudget);
    ASSERT_TRUE(std::holds_alternative<core::ConversationReply>(result));
    const auto revised = m_profiles.load();
    ASSERT_EQ(revised.facts.size(), 2U);
    EXPECT_EQ(revised.facts.front().supersededByFactIdentifier, revised.facts.back().identifier);
    EXPECT_EQ(revised.facts.back().replacesFactIdentifier, oldIdentifier);
    EXPECT_EQ(revised.facts.back().tags, oldProfile.facts.front().tags);
    EXPECT_TRUE(revised.facts.back().alwaysInclude);
}

/// @brief Независимое соединение проверяет видимость User и отсутствие блокировки записи во время LLM.
class ObservingLlmClient final : public core::LlmClient {
public:
    explicit ObservingLlmClient(const QString& path);

protected:
    [[nodiscard]] core::LlmResult requestCompletion(const core::LlmRequest& request) override;

private:
    QString m_path;
};

ObservingLlmClient::ObservingLlmClient(const QString& path)
    : m_path{path}
{
}

core::LlmResult ObservingLlmClient::requestCompletion([[maybe_unused]] const core::LlmRequest& request)
{
    SqliteDatabase observer{m_path, sqliteMigrations()};
    SqliteChatRepository chats{observer};
    const auto selected = chats.currentChatId();
    EXPECT_TRUE(selected);
    if (!selected) {
        throw std::runtime_error{"User не виден независимому соединению"};
    }
    const auto history = chats.messages(*selected);
    EXPECT_EQ(history.size(), 1U);
    if (history.empty()) {
        throw std::runtime_error{"История User не видна независимому соединению"};
    }
    EXPECT_EQ(history.front().role, core::MessageRole::User);
    SqliteGreetingRepository greetings{observer};
    greetings.saveDate(kDate);

    return core::LlmResponse{.reply = "Ответ"};
}

TEST_F(SqliteConversationTest, UserIsCommittedAndOtherConnectionCanWriteWhileModelRuns)
{
    ObservingLlmClient client{m_directory.filePath(QStringLiteral("чат.sqlite"))};
    core::ConversationService service{m_clock, m_chat, m_profile, m_daily, client,
        m_moodService, m_checkIn, m_checkIns, m_database};
    const auto result = service.sendMessage("Вопрос", kContextBudget);
    EXPECT_TRUE(std::holds_alternative<core::ConversationReply>(result));
    SqliteGreetingRepository greetings{m_database};
    EXPECT_EQ(greetings.lastDate(), kDate);
}

}  // namespace
}  // namespace msa::infra

