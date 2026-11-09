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

enum class ReminderFailure {
    Read,
    Add,
    MissingUpdate,
    ThrowingUpdate,
};

/// @brief Обёртка хранилища в памяти с отказом до изменения выбранной записи.
class FailingReminderRepository final : public ReminderRepository {
public:
    FailingReminderRepository(InMemoryReminderRepository& storage, ReminderFailure failure,
        std::int64_t failingIdentifier = 0);
    [[nodiscard]] std::int64_t add(const Reminder& item) override;
    [[nodiscard]] std::optional<Reminder> find(std::int64_t identifier) const override;
    [[nodiscard]] std::vector<Reminder> all() const override;
    [[nodiscard]] bool update(const Reminder& item) override;
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    InMemoryReminderRepository& m_storage;
    ReminderFailure m_failure;
    std::int64_t m_failingIdentifier{0};
};

FailingReminderRepository::FailingReminderRepository(InMemoryReminderRepository& storage,
    ReminderFailure failure, std::int64_t failingIdentifier)
    : m_storage{storage}, m_failure{failure}, m_failingIdentifier{failingIdentifier}
{
}

std::int64_t FailingReminderRepository::add(const Reminder& item)
{
    if (m_failure == ReminderFailure::Add) {
        throw std::runtime_error{"Ошибка создания"};
    }

    return m_storage.add(item);
}

std::optional<Reminder> FailingReminderRepository::find(std::int64_t identifier) const
{
    return m_storage.find(identifier);
}

std::vector<Reminder> FailingReminderRepository::all() const
{
    if (m_failure == ReminderFailure::Read) {
        throw std::runtime_error{"Ошибка чтения"};
    }

    return m_storage.all();
}

bool FailingReminderRepository::update(const Reminder& item)
{
    const auto identifier = std::visit([](const auto& value) { return value.identifier; }, item);
    if (identifier == m_failingIdentifier) {
        if (m_failure == ReminderFailure::MissingUpdate) {
            return false;
        }
        if (m_failure == ReminderFailure::ThrowingUpdate) {
            throw std::runtime_error{"Ошибка обновления"};
        }
    }

    return m_storage.update(item);
}

bool FailingReminderRepository::remove(std::int64_t identifier)
{
    return m_storage.remove(identifier);
}

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

TEST(ReminderServiceTest, ActiveListIsEmptyWithoutPendingRecords)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    const ReminderService service{clock, repository};
    EXPECT_TRUE(service.activeReminders().empty());
    for (const auto status : {ReminderStatus::Fired, ReminderStatus::Missed}) {
        static_cast<void>(repository.add(TimedReminder{.status = status}));
        static_cast<void>(repository.add(PlaceReminder{.status = status}));
    }
    EXPECT_TRUE(service.activeReminders().empty());
    EXPECT_EQ(repository.all().size(), 4U);
}

TEST(ReminderServiceTest, ActiveListOrdersMixedRecordsByTimeThenPlacesAndIdentifier)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    const ReminderService service{clock, repository};
    const auto place = repository.add(PlaceReminder{.title = "Место", .place = "Дом"});
    const auto later = repository.add(TimedReminder{.dueAt = noon() + minutes{1}});
    const auto recurring = repository.add(RecurringReminder{
        .dueAt = noon(), .interval = minutes{45}});
    const auto timed = repository.add(TimedReminder{.dueAt = noon()});
    const auto past = repository.add(TimedReminder{.dueAt = noon() - seconds{1}});
    const auto located = repository.add(PlaceReminder{
        .title = "Координаты", .place = "Парк", .coordinates = GeoCoordinates{0.0, 0.0}});
    static_cast<void>(repository.add(TimedReminder{.status = ReminderStatus::Fired}));
    static_cast<void>(repository.add(PlaceReminder{.status = ReminderStatus::Missed}));

    const auto active = service.activeReminders();
    ASSERT_EQ(active.size(), 6U);
    EXPECT_EQ(std::get<TimedReminder>(active[0]).identifier, past);
    EXPECT_EQ(std::get<RecurringReminder>(active[1]).identifier, recurring);
    EXPECT_EQ(std::get<TimedReminder>(active[2]).identifier, timed);
    EXPECT_EQ(std::get<TimedReminder>(active[3]).identifier, later);
    EXPECT_EQ(std::get<PlaceReminder>(active[4]).identifier, place);
    EXPECT_EQ(std::get<PlaceReminder>(active[5]).identifier, located);
    EXPECT_FALSE(std::get<PlaceReminder>(active[4]).coordinates.has_value());
}

TEST(ReminderServiceTest, ActiveListReturnsCopiesWithoutProcessingDueRecords)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository repository;
    ReminderService service{clock, repository};
    const auto timed = repository.add(TimedReminder{.title = "Звонок", .dueAt = noon()});
    const auto recurring = repository.add(RecurringReminder{
        .title = "Вода", .dueAt = noon(), .interval = minutes{45}});
    clock.advance(minutes{1});

    auto active = service.activeReminders();
    ASSERT_EQ(active.size(), 2U);
    std::get<TimedReminder>(active.front()).title = "Изменённая копия";
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(timed)).title, "Звонок");
    EXPECT_EQ(std::get<TimedReminder>(*repository.find(timed)).status, ReminderStatus::Pending);
    EXPECT_EQ(std::get<RecurringReminder>(*repository.find(recurring)).dueAt, noon());

    ASSERT_EQ(service.checkDue().size(), 2U);
    const auto afterFiring = service.activeReminders();
    ASSERT_EQ(afterFiring.size(), 1U);
    EXPECT_EQ(std::get<RecurringReminder>(afterFiring.front()).dueAt, clock.now() + minutes{45});
    ASSERT_TRUE(repository.remove(recurring));
    EXPECT_TRUE(service.activeReminders().empty());
}

TEST(ReminderServiceTest, PropagatesReadAndCreateFailuresWithoutChangingStorage)
{
    FakeClock clock{noon()};
    InMemoryReminderRepository storage;
    FailingReminderRepository unreadable{storage, ReminderFailure::Read};
    ReminderService reader{clock, unreadable};
    EXPECT_THROW(static_cast<void>(reader.activeReminders()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(reader.checkDue()), std::runtime_error);
    EXPECT_THROW(reader.skipMissedOnStartup(), std::runtime_error);
    EXPECT_THROW(static_cast<void>(reader.checkNearPlace({0.0, 0.0})), std::runtime_error);

    FailingReminderRepository unwritable{storage, ReminderFailure::Add};
    ReminderService writer{clock, unwritable};
    EXPECT_THROW(static_cast<void>(writer.addTimed("Звонок", noon())), std::runtime_error);
    EXPECT_THROW(static_cast<void>(writer.addRecurring("Вода", minutes{45})), std::runtime_error);
    EXPECT_THROW(static_cast<void>(writer.addPlace("Покупки", "Магазин")), std::runtime_error);
    EXPECT_TRUE(storage.all().empty());
}

TEST(ReminderServiceTest, DueFailureKeepsEarlierUpdateAndRetryDoesNotReplayIt)
{
    for (const auto failure : {ReminderFailure::MissingUpdate, ReminderFailure::ThrowingUpdate}) {
        FakeClock clock{noon()};
        InMemoryReminderRepository storage;
        const auto first = storage.add(TimedReminder{.dueAt = noon()});
        const auto second = storage.add(RecurringReminder{.dueAt = noon(), .interval = minutes{45}});
        const auto third = storage.add(TimedReminder{.dueAt = noon()});
        FailingReminderRepository repository{storage, failure, second};
        ReminderService service{clock, repository};

        EXPECT_THROW(static_cast<void>(service.checkDue()), std::runtime_error);
        EXPECT_EQ(std::get<TimedReminder>(*storage.find(first)).status, ReminderStatus::Fired);
        EXPECT_EQ(std::get<RecurringReminder>(*storage.find(second)).dueAt, noon());
        EXPECT_EQ(std::get<TimedReminder>(*storage.find(third)).status, ReminderStatus::Pending);

        ReminderService recovered{clock, storage};
        const auto retried = recovered.checkDue();
        ASSERT_EQ(retried.size(), 2U);
        EXPECT_EQ(std::get<RecurringReminder>(retried[0]).identifier, second);
        EXPECT_EQ(std::get<TimedReminder>(retried[1]).identifier, third);
        EXPECT_EQ(std::get<RecurringReminder>(*storage.find(second)).dueAt, noon() + minutes{45});
        EXPECT_TRUE(recovered.checkDue().empty());
    }
}

TEST(ReminderServiceTest, StartupFailureKeepsEarlierMissedStatusAndCanBeRetried)
{
    for (const auto failure : {ReminderFailure::MissingUpdate, ReminderFailure::ThrowingUpdate}) {
        FakeClock clock{noon()};
        InMemoryReminderRepository storage;
        const auto first = storage.add(TimedReminder{.dueAt = noon() - seconds{1}});
        const auto second = storage.add(TimedReminder{.dueAt = noon() - seconds{1}});
        const auto current = storage.add(TimedReminder{.dueAt = noon()});
        FailingReminderRepository repository{storage, failure, second};
        ReminderService service{clock, repository};

        EXPECT_THROW(service.skipMissedOnStartup(), std::runtime_error);
        EXPECT_EQ(std::get<TimedReminder>(*storage.find(first)).status, ReminderStatus::Missed);
        EXPECT_EQ(std::get<TimedReminder>(*storage.find(second)).status, ReminderStatus::Pending);
        ReminderService recovered{clock, storage};
        recovered.skipMissedOnStartup();
        EXPECT_EQ(std::get<TimedReminder>(*storage.find(second)).status, ReminderStatus::Missed);
        const auto due = recovered.checkDue();
        ASSERT_EQ(due.size(), 1U);
        EXPECT_EQ(std::get<TimedReminder>(due.front()).identifier, current);
    }
}

TEST(ReminderServiceTest, PlaceFailureKeepsEarlierFiredStatusAndRetryDoesNotReplayIt)
{
    for (const auto failure : {ReminderFailure::MissingUpdate, ReminderFailure::ThrowingUpdate}) {
        FakeClock clock{noon()};
        InMemoryReminderRepository storage;
        const auto first = storage.add(PlaceReminder{.coordinates = GeoCoordinates{0.0, 0.0}});
        const auto second = storage.add(PlaceReminder{.coordinates = GeoCoordinates{0.0, 0.0}});
        FailingReminderRepository repository{storage, failure, second};
        ReminderService service{clock, repository};

        EXPECT_THROW(static_cast<void>(service.checkNearPlace({0.0, 0.0})), std::runtime_error);
        EXPECT_EQ(std::get<PlaceReminder>(*storage.find(first)).status, ReminderStatus::Fired);
        EXPECT_EQ(std::get<PlaceReminder>(*storage.find(second)).status, ReminderStatus::Pending);
        ReminderService recovered{clock, storage};
        const auto retried = std::get<std::vector<PlaceReminder>>(recovered.checkNearPlace({0.0, 0.0}));
        ASSERT_EQ(retried.size(), 1U);
        EXPECT_EQ(retried.front().identifier, second);
        EXPECT_TRUE(std::get<std::vector<PlaceReminder>>(recovered.checkNearPlace({0.0, 0.0})).empty());
    }
}

}  // namespace
}  // namespace msa::core
