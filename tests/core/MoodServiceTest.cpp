#include "FakeClock.h"
#include "InMemoryMoodRepository.h"

#include "msa/core/MoodService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

using std::chrono::days;
using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::seconds;
using std::chrono::year;

class MoodServiceTest : public ::testing::Test {
protected:
    FakeClock m_clock{local_days{year{2026} / 11 / 6} + hours{12}};
    InMemoryMoodRepository m_repository{};
    MoodService m_service{m_clock, m_repository};
};

TEST_F(MoodServiceTest, ExplicitlyRecordsAllScoresWithFullTextAndCurrentTime)
{
    const std::string note = "  Серьёзное событие: " + std::string(200, 'x') + "\nПодробности 🙂  ";
    const auto startedAt = m_clock.now();
    std::vector<std::int64_t> identifiers;
    for (const auto score : {MoodScore::Negative, MoodScore::Neutral, MoodScore::Positive}) {
        const auto result = m_service.logMood(score, note);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result));
        identifiers.push_back(std::get<std::int64_t>(result));
        m_clock.advance(seconds{1});
    }
    const auto entries = std::get<std::vector<MoodEntry>>(m_service.moodHistory());
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].score, MoodScore::Negative);
    EXPECT_EQ(entries[1].score, MoodScore::Neutral);
    EXPECT_EQ(entries[2].score, MoodScore::Positive);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        EXPECT_GT(identifiers[index], 0);
        EXPECT_EQ(entries[index].identifier, identifiers[index]);
        EXPECT_EQ(entries[index].recordedAt, startedAt + seconds{static_cast<seconds::rep>(index)});
        EXPECT_EQ(entries[index].note, note);
    }
    EXPECT_NE(identifiers[0], identifiers[1]);
    EXPECT_NE(identifiers[1], identifiers[2]);
}

TEST_F(MoodServiceTest, AllowsEmptyNotesAndKeepsRepeatedExplicitRecords)
{
    static_cast<void>(m_service.logMood(MoodScore::Neutral));
    static_cast<void>(m_service.logMood(MoodScore::Neutral));
    const auto entries = std::get<std::vector<MoodEntry>>(m_service.moodHistory());
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_TRUE(entries[0].note.empty());
    EXPECT_TRUE(entries[1].note.empty());
    EXPECT_NE(entries[0].identifier, entries[1].identifier);
}

TEST_F(MoodServiceTest, DefaultHistoryIncludesFourteenCalendarDaysAndExcludesFutureWithoutDeleting)
{
    const auto firstDay = local_days{year{2026} / 10 / 24};
    const auto old = m_repository.add(MoodEntry{.recordedAt = firstDay - seconds{1}, .note = "Важное"});
    const auto first = m_repository.add(MoodEntry{.recordedAt = firstDay});
    const auto current = m_repository.add(MoodEntry{.recordedAt = m_clock.now()});
    static_cast<void>(m_repository.add(MoodEntry{.recordedAt = m_clock.now() + seconds{1}}));
    const auto recent = std::get<std::vector<MoodEntry>>(m_service.moodHistory());
    ASSERT_EQ(recent.size(), 2U);
    EXPECT_EQ(recent[0].identifier, first);
    EXPECT_EQ(recent[1].identifier, current);
    const auto extended = std::get<std::vector<MoodEntry>>(m_service.moodHistory(15));
    ASSERT_EQ(extended.size(), 3U);
    EXPECT_EQ(extended.front().identifier, old);
    EXPECT_EQ(extended.front().note, "Важное");
    EXPECT_EQ(m_repository.all().size(), 4U);
}

TEST_F(MoodServiceTest, OneDayHistoryStartsAtMidnightAcrossYearAndLeapBoundaries)
{
    for (const auto date : {year{2027} / 1 / 1, year{2024} / 3 / 1, year{2025} / 3 / 1}) {
        InMemoryMoodRepository repository;
        MoodService service{m_clock, repository};
        const auto midnight = local_days{date};
        m_clock.set(midnight + hours{12});
        static_cast<void>(repository.add(MoodEntry{.recordedAt = midnight - seconds{1}}));
        const auto first = repository.add(MoodEntry{.recordedAt = midnight});
        const auto current = repository.add(MoodEntry{.recordedAt = m_clock.now()});
        const auto history = std::get<std::vector<MoodEntry>>(service.moodHistory(1));
        ASSERT_EQ(history.size(), 2U);
        EXPECT_EQ(history[0].identifier, first);
        EXPECT_EQ(history[1].identifier, current);
        m_clock.set(midnight + days{1});
        EXPECT_TRUE(std::get<std::vector<MoodEntry>>(service.moodHistory(1)).empty());
        EXPECT_EQ(std::get<std::vector<MoodEntry>>(service.moodHistory(2)).size(), 2U);
        EXPECT_EQ(std::get<std::vector<MoodEntry>>(service.moodHistory(3)).size(), 3U);
        EXPECT_EQ(repository.all().size(), 3U);
    }
}

TEST_F(MoodServiceTest, SortsByTimestampThenIdentifierAndReturnsIndependentCopies)
{
    const auto later = m_repository.add(MoodEntry{.recordedAt = m_clock.now(), .note = "Позже"});
    const auto first = m_repository.add(MoodEntry{.recordedAt = m_clock.now() - seconds{1}, .note = "Раньше"});
    const auto tied = m_repository.add(MoodEntry{.recordedAt = m_clock.now() - seconds{1}});
    auto history = std::get<std::vector<MoodEntry>>(m_service.moodHistory());
    ASSERT_EQ(history.size(), 3U);
    EXPECT_EQ(history[0].identifier, first);
    EXPECT_EQ(history[1].identifier, tied);
    EXPECT_EQ(history[2].identifier, later);
    history[0].note = "Изменённая копия";
    const auto repeated = std::get<std::vector<MoodEntry>>(m_service.moodHistory());
    EXPECT_EQ(repeated[0].note, "Раньше");
    EXPECT_EQ(m_repository.all().size(), 3U);
}

TEST_F(MoodServiceTest, PreservesOldRecordsAcrossServiceRecreationAndWiderHistoryQueries)
{
    EXPECT_TRUE(std::get<std::vector<MoodEntry>>(m_service.moodHistory()).empty());
    const auto identifier = std::get<std::int64_t>(m_service.logMood(MoodScore::Negative, "Важное событие"));
    m_clock.advance(days{30});
    MoodService recreated{m_clock, m_repository};
    EXPECT_TRUE(std::get<std::vector<MoodEntry>>(recreated.moodHistory()).empty());
    const auto history = std::get<std::vector<MoodEntry>>(
        recreated.moodHistory(std::numeric_limits<int>::max()));
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history.front().identifier, identifier);
    EXPECT_EQ(history.front().note, "Важное событие");
    EXPECT_EQ(m_repository.all().size(), 1U);
}

/// @brief Хранилище с техническим отказом для проверки валидации до обращения к данным.
class FailingMoodRepository final : public MoodRepository {
public:
    [[nodiscard]] std::int64_t add(const MoodEntry& entry) override;
    [[nodiscard]] std::vector<MoodEntry> all() const override;
};

std::int64_t FailingMoodRepository::add([[maybe_unused]] const MoodEntry& entry)
{
    throw std::runtime_error{"Ошибка сохранения настроения"};
}

std::vector<MoodEntry> FailingMoodRepository::all() const
{
    throw std::runtime_error{"Ошибка чтения дневника"};
}

TEST_F(MoodServiceTest, RejectsInvalidScoreAndNonpositiveDaysBeforeAccessingStorage)
{
    FailingMoodRepository repository;
    MoodService service{m_clock, repository};
    const auto invalidScore = service.logMood(static_cast<MoodScore>(2), "Текст");
    ASSERT_TRUE(std::holds_alternative<MoodRejection>(invalidScore));
    EXPECT_FALSE(std::get<MoodRejection>(invalidScore).reason.empty());
    for (const auto period : {0, -1, std::numeric_limits<int>::min()}) {
        const auto result = service.moodHistory(period);
        ASSERT_TRUE(std::holds_alternative<MoodRejection>(result));
        EXPECT_FALSE(std::get<MoodRejection>(result).reason.empty());
    }
}

TEST_F(MoodServiceTest, PropagatesStorageErrorsForValidRequests)
{
    FailingMoodRepository repository;
    MoodService service{m_clock, repository};
    EXPECT_THROW(static_cast<void>(service.logMood(MoodScore::Neutral, "Запись")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(service.moodHistory()), std::runtime_error);
}

TEST(MoodRepositoryTest, StoresIndependentCopiesWithAssignedIdentifiers)
{
    InMemoryMoodRepository memory;
    MoodRepository& repository = memory;
    EXPECT_TRUE(repository.all().empty());
    MoodEntry original{.recordedAt = local_days{year{2026} / 11 / 6},
        .score = MoodScore::Positive, .note = "Полный текст"};
    const auto identifier = repository.add(original);
    EXPECT_GT(identifier, 0);
    EXPECT_EQ(original.identifier, 0);
    original.note.clear();
    auto entries = repository.all();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().identifier, identifier);
    EXPECT_EQ(entries.front().note, "Полный текст");
    EXPECT_EQ(entries.front().score, MoodScore::Positive);
    EXPECT_EQ(entries.front().recordedAt, original.recordedAt);
    EXPECT_THROW(static_cast<void>(repository.add(entries.front())), std::invalid_argument);
    entries.front().note.clear();
    EXPECT_EQ(repository.all().front().note, "Полный текст");
    EXPECT_EQ(repository.all().size(), 1U);
}

}  // namespace
}  // namespace msa::core

