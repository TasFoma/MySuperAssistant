#include "FakeClock.h"
#include "FakeLlmClient.h"
#include "FakeWebSearchProvider.h"

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
    core::FakeWebSearchProvider m_search{};
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
        m_moodService, m_checkIn, m_checkIns, m_database, m_search};
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
        m_moodService, m_checkIn, m_checkIns, m_database, m_search};
    const auto result = service.sendMessage("Вопрос", kContextBudget);
    EXPECT_TRUE(std::holds_alternative<core::ConversationReply>(result));
    SqliteGreetingRepository greetings{m_database};
    EXPECT_EQ(greetings.lastDate(), kDate);
}

TEST_F(SqliteConversationTest, CheckInWriteFailuresRestorePreviousPublicationAndAllowExplicitRetry)
{
    const std::array triggers{
        "CREATE TRIGGER fail_offer BEFORE INSERT ON evening_check_in_state "
            "BEGIN SELECT RAISE(ABORT, 'date failure'); END",
        "CREATE TRIGGER fail_offer BEFORE INSERT ON chat_messages "
            "BEGIN SELECT RAISE(ABORT, 'message failure'); END",
        "CREATE TRIGGER fail_offer BEFORE INSERT ON pending_evening_check_in "
            "BEGIN SELECT RAISE(ABORT, 'pending failure'); END",
        "CREATE TRIGGER fail_offer BEFORE DELETE ON pending_evening_check_in "
            "BEGIN SELECT RAISE(ABORT, 'expiry failure'); END"};
    auto service = conversation();
    for (const auto* trigger : triggers) {
        SCOPED_TRACE(trigger);
        m_clock.advance(std::chrono::days{1});
        const auto previousDate = m_checkIns.lastDate();
        const auto previousPending = m_checkIns.pendingCheckIn();
        const auto history = m_chat.loadMessages();
        m_database.execute(QString::fromUtf8(trigger));
        EXPECT_THROW(static_cast<void>(service.offerEveningCheckIn()), std::runtime_error);
        EXPECT_EQ(m_checkIns.lastDate(), previousDate);
        const auto afterFailure = m_checkIns.pendingCheckIn();
        ASSERT_EQ(afterFailure.has_value(), previousPending.has_value());
        if (previousPending) {
            EXPECT_EQ(afterFailure->chatIdentifier, previousPending->chatIdentifier);
            EXPECT_EQ(afterFailure->messageIdentifier, previousPending->messageIdentifier);
            EXPECT_EQ(afterFailure->date, previousPending->date);
        }
        const auto afterHistory = m_chat.loadMessages();
        ASSERT_EQ(afterHistory.size(), history.size());
        if (!history.empty()) {
            EXPECT_EQ(afterHistory.back().identifier, history.back().identifier);
            EXPECT_EQ(afterHistory.back().text, history.back().text);
        }
        m_database.execute(QStringLiteral("DROP TRIGGER fail_offer"));
        const auto retry = service.offerEveningCheckIn();
        ASSERT_TRUE(retry);
        EXPECT_EQ(m_chat.loadMessages().size(), history.size() + 1);
        ASSERT_TRUE(m_checkIns.pendingCheckIn());
        EXPECT_EQ(m_checkIns.pendingCheckIn()->messageIdentifier, retry);
        EXPECT_FALSE(service.offerEveningCheckIn());
    }
    EXPECT_TRUE(m_client.requests().empty());
    EXPECT_TRUE(m_moods.all().empty());
    EXPECT_TRUE(m_profiles.load().facts.empty());
}

TEST_F(SqliteConversationTest, CheckInCommitFailureLeavesNoDateMessageOrPendingAndCanBeRetried)
{
    auto service = conversation();
    m_database.execute(QStringLiteral("CREATE TABLE offer_commit_probe ("
        "identifier INTEGER REFERENCES chats(identifier) DEFERRABLE INITIALLY DEFERRED)"));
    m_database.execute(QStringLiteral("CREATE TRIGGER fail_offer_commit AFTER INSERT ON pending_evening_check_in "
        "BEGIN INSERT INTO offer_commit_probe VALUES (999999); END"));
    EXPECT_THROW(static_cast<void>(service.offerEveningCheckIn()), std::runtime_error);
    EXPECT_FALSE(m_checkIns.lastDate());
    EXPECT_FALSE(m_checkIns.pendingCheckIn());
    EXPECT_TRUE(m_chat.loadMessages().empty());
    m_database.execute(QStringLiteral("DROP TRIGGER fail_offer_commit"));
    EXPECT_TRUE(service.offerEveningCheckIn());
    EXPECT_EQ(m_chat.loadMessages().size(), 1U);
    EXPECT_FALSE(service.offerEveningCheckIn());
    EXPECT_TRUE(m_client.requests().empty());
}

TEST_F(SqliteConversationTest, CheckInPublicationSurvivesReopenWithoutRepeatingOrMovingBetweenChats)
{
    auto service = conversation();
    const auto originalChat = m_chat.currentChatId();
    const auto prompt = service.offerEveningCheckIn();
    ASSERT_TRUE(prompt);
    const auto otherChat = m_chat.newChat();
    EXPECT_FALSE(service.offerEveningCheckIn());
    SqliteDatabase reopened{m_directory.filePath(QStringLiteral("чат.sqlite")), sqliteMigrations()};
    SqliteChatRepository chats{reopened};
    core::ChatService restoredChat{m_clock, chats};
    EXPECT_EQ(restoredChat.currentChatId(), otherChat);
    SqliteEveningCheckInRepository checkIns{reopened};
    EXPECT_EQ(checkIns.lastDate(), kDate);
    const auto pending = checkIns.pendingCheckIn();
    ASSERT_TRUE(pending);
    EXPECT_EQ(pending->chatIdentifier, originalChat);
    EXPECT_EQ(pending->messageIdentifier, prompt);
    EXPECT_EQ(pending->date, kDate);
    ASSERT_EQ(chats.messages(originalChat).size(), 1U);
    EXPECT_EQ(chats.messages(originalChat).front().identifier, prompt);
    EXPECT_TRUE(chats.messages(otherChat).empty());
    SqliteUserProfileRepository profiles{reopened};
    core::UserProfileService profile{profiles};
    SqliteEventRepository events{reopened};
    core::CalendarService calendar{events};
    SqliteTaskRepository tasks{reopened};
    core::TaskService taskService{m_clock, tasks};
    SqliteHabitRepository habits{reopened};
    core::HabitService habitService{m_clock, habits};
    core::DailyContextService daily{m_clock, calendar, taskService, habitService};
    SqliteMoodRepository moods{reopened};
    core::MoodService moodService{m_clock, moods};
    core::EveningCheckInService restoredCheckIn{m_clock, checkIns};
    core::ConversationService restored{m_clock, restoredChat, profile, daily, m_client,
        moodService, restoredCheckIn, checkIns, reopened, m_search};
    EXPECT_FALSE(restored.offerEveningCheckIn());
    m_clock.advance(std::chrono::days{1});
    const auto nextPrompt = restored.offerEveningCheckIn();
    ASSERT_TRUE(nextPrompt);
    ASSERT_TRUE(checkIns.pendingCheckIn());
    EXPECT_EQ(checkIns.pendingCheckIn()->chatIdentifier, otherChat);
    EXPECT_EQ(chats.messages(originalChat).size(), 1U);
    EXPECT_EQ(chats.messages(otherChat).size(), 1U);
    EXPECT_TRUE(m_client.requests().empty());
}

TEST_F(SqliteConversationTest, SearchRunsAfterCommittedUserAndSourcesSurviveReopening)
{
    m_client.setResult(core::LlmResponse{.reply = "Ответ по источнику [1]"});
    m_search.result = std::vector<core::WebSearchResult>{{"Материал", "https://example.org/source", "Описание"}};
    m_search.onSearch = [this]() {
        SqliteDatabase observer{m_directory.filePath(QStringLiteral("чат.sqlite")), sqliteMigrations()};
        SqliteChatRepository chats{observer};
        core::ChatService chat{m_clock, chats};
        ASSERT_EQ(chat.loadMessages().size(), 1);
        EXPECT_EQ(chat.loadMessages().front().text, "  Полный личный вопрос  ");
        SqliteGreetingRepository greetings{observer};
        greetings.saveDate(kDate);
    };
    auto service = conversation();
    const auto result = service.sendMessageWithSearch("  Полный личный вопрос  ", "Поисковая фраза", kContextBudget);
    ASSERT_TRUE(std::holds_alternative<core::ConversationReply>(result));
    EXPECT_EQ(m_search.queries, (std::vector<std::string>{"Поисковая фраза"}));
    SqliteDatabase reopened{m_directory.filePath(QStringLiteral("чат.sqlite")), sqliteMigrations()};
    SqliteChatRepository chats{reopened};
    core::ChatService restored{m_clock, chats};
    ASSERT_EQ(restored.loadMessages().size(), 2);
    EXPECT_NE(restored.loadMessages().back().text.find("https://example.org/source"), std::string::npos);
    SqliteGreetingRepository greetings{reopened};
    EXPECT_EQ(greetings.lastDate(), kDate);
}

}  // namespace
}  // namespace msa::infra

