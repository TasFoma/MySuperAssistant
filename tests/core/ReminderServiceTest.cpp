#include "FakeClock.h"
#include "InMemoryReminderRepository.h"
#include "msa/core/ReminderService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <variant>

namespace msa::core {
namespace {

using std::chrono::minutes;
using std::chrono::seconds;

[[nodiscard]] std::chrono::local_time<seconds> noon()
{
    return std::chrono::local_days{std::chrono::year{2026} / 11 / 5}
           + std::chrono::hours{12};
}

TEST(ReminderServiceTest, RejectsPastAndInvalidIntervalsWithoutSaving)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto past = service.addTimed("Звонок", noon() - seconds{1});
    ASSERT_TRUE(std::holds_alternative<ReminderRejection>(past));
    EXPECT_FALSE(std::get<ReminderRejection>(past).reason.empty());
    for (const auto interval : {minutes{0}, minutes{-1}, minutes::max()}) {
        const auto result = service.addRecurring("Вода", interval);
        ASSERT_TRUE(std::holds_alternative<ReminderRejection>(result));
        EXPECT_FALSE(std::get<ReminderRejection>(result).reason.empty());
    }
    EXPECT_TRUE(repository.all().empty());
}

TEST(ReminderServiceTest, FiresAtExactTimeOnceAndLeavesFutureAndPlacesAlone)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto result = service.addTimed("Звонок", noon() + minutes{1});
    const auto identifier = std::get<std::int64_t>(result);
    const auto future = service.addTimed("Позже", noon() + minutes{2});
    const auto placeIdentifier = repository.add(PlaceReminder{.title = "Магазин"});
    EXPECT_TRUE(service.checkDue().empty());
    clock.advance(minutes{1});
    const auto due = service.checkDue();
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(std::get<TimedReminder>(due.front()).identifier, identifier);
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(identifier)).status, ReminderStatus::Fired);
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(std::get<std::int64_t>(future))).status,
              ReminderStatus::Pending);
    EXPECT_EQ(std::get<PlaceReminder>(*repository.find(placeIdentifier)).status, ReminderStatus::Pending);
    EXPECT_TRUE(service.checkDue().empty());
}

TEST(ReminderServiceTest, StartupSkipsOnlyPastPendingOneShots)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto past = repository.add(TimedReminder{.title = "Пропущено", .dueAt = noon() - minutes{1}});
    const auto fired = repository.add(TimedReminder{.title = "Показано", .dueAt = noon() - minutes{1},
                                                   .status = ReminderStatus::Fired});
    const auto current = service.addTimed("Сейчас", noon());
    service.skipMissedOnStartup();
    service.skipMissedOnStartup();
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(past)).status, ReminderStatus::Missed);
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(fired)).status, ReminderStatus::Fired);
    const auto due = service.checkDue();
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(std::get<TimedReminder>(due.front()).identifier, std::get<std::int64_t>(current));
}

TEST(ReminderServiceTest, RecurringSkipsMissedIntervalsAndSchedulesFromNow)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto identifier = std::get<std::int64_t>(service.addRecurring("Вода", minutes{45}));
    EXPECT_EQ(std::get<RecurringReminder>(*repository.find(identifier)).dueAt, noon() + minutes{45});
    EXPECT_TRUE(service.checkDue().empty());
    clock.advance(std::chrono::hours{4});
    service.skipMissedOnStartup();
    ASSERT_EQ(service.checkDue().size(), 1U);
    EXPECT_EQ(std::get<RecurringReminder>(*repository.find(identifier)).dueAt, clock.now() + minutes{45});
    EXPECT_TRUE(service.checkDue().empty());
    clock.advance(minutes{45});
    EXPECT_EQ(service.checkDue().size(), 1U);
}

TEST(ReminderServiceTest, CorruptStoredIntervalThrowsWithoutChangingRecord)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto identifier = repository.add(RecurringReminder{.title = "Вода", .dueAt = noon()});
    EXPECT_THROW(static_cast<void>(service.checkDue()), std::runtime_error);
    EXPECT_EQ(std::get<RecurringReminder>(*repository.find(identifier)).dueAt, noon());
}

}  // namespace
}  // namespace msa::core
