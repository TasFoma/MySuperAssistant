#include "FakeClock.h"
#include "InMemoryEveningCheckInRepository.h"

#include "msa/core/EveningCheckInService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>

namespace msa::core {
namespace {

using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::year;

TEST(EveningCheckInServiceTest, PreparesAtFirstCheckFromTwentyUntilEndOfDay)
{
    const auto date = year{2026} / 11 / 6;
    for (const auto time : {seconds{hours{20}}, seconds{hours{21}},
                           seconds{hours{23} + minutes{59} + seconds{59}}}) {
        FakeClock clock{local_days{date} + time};
        InMemoryEveningCheckInRepository repository;
        EXPECT_FALSE(repository.lastDate());
        EveningCheckInService service{clock, repository};
        const auto message = service.checkInIfNeeded();
        ASSERT_TRUE(message);
        EXPECT_EQ(*message, "Как прошёл день? 🌙 Если хочешь, расскажи о настроении "
            "и важных событиях — сохраню твой ответ в дневник настроения.");
        EXPECT_EQ(repository.lastDate(), date);
        EXPECT_FALSE(service.checkInIfNeeded());
    }
}

TEST(EveningCheckInServiceTest, DoesNotChangeSavedDateBeforeTwentyAndFiresAtBoundary)
{
    const auto yesterday = year{2026} / 11 / 5;
    FakeClock clock{local_days{year{2026} / 11 / 6}};
    InMemoryEveningCheckInRepository repository;
    repository.saveDate(yesterday);
    EveningCheckInService service{clock, repository};
    EXPECT_FALSE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), yesterday);
    clock.advance(hours{20} - seconds{1});
    EXPECT_FALSE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), yesterday);
    clock.advance(seconds{1});
    EXPECT_TRUE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2026} / 11 / 6);
}

TEST(EveningCheckInServiceTest, SavedDatePreventsRepeatAcrossServicesAndResetsOnNewDay)
{
    FakeClock clock{local_days{year{2026} / 12 / 31} + hours{20}};
    InMemoryEveningCheckInRepository repository;
    EveningCheckInService service{clock, repository};
    ASSERT_TRUE(service.checkInIfNeeded());
    clock.advance(hours{3} + minutes{59} + seconds{59});
    EveningCheckInService restarted{clock, repository};
    EXPECT_FALSE(restarted.checkInIfNeeded());
    clock.advance(seconds{1});
    EXPECT_FALSE(restarted.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2026} / 12 / 31);
    clock.advance(hours{20});
    EXPECT_TRUE(restarted.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2027} / 1 / 1);
    EXPECT_FALSE(service.checkInIfNeeded());
}

TEST(EveningCheckInServiceTest, SkipsMissedDaysAndOnlyPreparesForCurrentEvening)
{
    FakeClock clock{local_days{year{2026} / 11 / 9} + hours{10}};
    InMemoryEveningCheckInRepository repository;
    repository.saveDate(year{2026} / 11 / 1);
    EveningCheckInService service{clock, repository};
    EXPECT_FALSE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2026} / 11 / 1);
    clock.advance(hours{12});
    EXPECT_TRUE(service.checkInIfNeeded());
    EXPECT_EQ(repository.lastDate(), year{2026} / 11 / 9);
    EXPECT_FALSE(service.checkInIfNeeded());
}

/// @brief Хранилище с отказом чтения или сохранения для проверки ошибок сервиса.
class FailingEveningCheckInRepository final : public EveningCheckInRepository {
public:
    explicit FailingEveningCheckInRepository(bool failOnLoad);
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override;
    void saveDate(std::chrono::year_month_day date) override;
    [[nodiscard]] std::optional<PendingEveningCheckIn> pendingCheckIn() const override;
    void savePendingCheckIn(std::optional<PendingEveningCheckIn> pending) override;

private:
    bool m_failOnLoad{true};
};

FailingEveningCheckInRepository::FailingEveningCheckInRepository(bool failOnLoad)
    : m_failOnLoad{failOnLoad}
{
}

std::optional<std::chrono::year_month_day> FailingEveningCheckInRepository::lastDate() const
{
    if (m_failOnLoad) {
        throw std::runtime_error{"Ошибка чтения"};
    }

    return std::nullopt;
}

void FailingEveningCheckInRepository::saveDate([[maybe_unused]] std::chrono::year_month_day date)
{
    throw std::runtime_error{"Ошибка сохранения"};
}

std::optional<PendingEveningCheckIn> FailingEveningCheckInRepository::pendingCheckIn() const
{
    throw std::runtime_error{"Ошибка чтения ожидания"};
}

void FailingEveningCheckInRepository::savePendingCheckIn(
    [[maybe_unused]] std::optional<PendingEveningCheckIn> pending)
{
    throw std::runtime_error{"Ошибка сохранения ожидания"};
}

TEST(EveningCheckInServiceTest, DoesNotAccessStorageBeforeTwentyAndPropagatesErrorsInWindow)
{
    FakeClock clock{local_days{year{2026} / 11 / 6} + hours{19}};
    for (const bool failOnLoad : {true, false}) {
        FailingEveningCheckInRepository repository{failOnLoad};
        EveningCheckInService service{clock, repository};
        clock.set(local_days{year{2026} / 11 / 6} + hours{19});
        EXPECT_FALSE(service.checkInIfNeeded());
        clock.advance(hours{1});
        EXPECT_THROW(static_cast<void>(service.checkInIfNeeded()), std::runtime_error);
    }
}

}  // namespace
}  // namespace msa::core

