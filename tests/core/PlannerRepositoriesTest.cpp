#include "FakeClock.h"
#include "InMemoryEventRepository.h"
#include "InMemoryHabitRepository.h"
#include "InMemoryReminderRepository.h"
#include "InMemoryTaskRepository.h"
#include "InMemoryUserProfileRepository.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

using namespace msa::core;

constexpr std::int64_t kMissingIdentifier = 999;

template <typename ItemType>
void setTitle(ItemType& item, const std::string& title)
{
    if constexpr (requires { item.title; }) {
        item.title = title;
    } else if constexpr (requires { item.name; }) {
        item.name = title;
    } else {
        std::visit([title](auto& value) { value.title = title; }, item);
    }
}

template <typename ItemType>
[[nodiscard]] std::string titleOf(const ItemType& item)
{
    if constexpr (requires { item.title; }) {
        return item.title;
    } else if constexpr (requires { item.name; }) {
        return item.name;
    } else {
        return std::visit([](const auto& value) { return value.title; }, item);
    }
}

// Общий контракт проверяется через интерфейс, чтобы те же сценарии подходили SQLite-реализациям.
template <typename RepositoryType, typename ItemType>
void checkRepositoryContract(RepositoryType& repository, ItemType item)
{
    EXPECT_TRUE(repository.all().empty());
    EXPECT_FALSE(repository.find(kMissingIdentifier).has_value());
    EXPECT_FALSE(repository.update(item));
    EXPECT_FALSE(repository.remove(kMissingIdentifier));
    EXPECT_TRUE(repository.all().empty());

    setTitle(item, "Исходная запись");
    const auto firstIdentifier = repository.add(item);
    const auto secondIdentifier = repository.add(item);
    EXPECT_GT(firstIdentifier, 0);
    EXPECT_GT(secondIdentifier, 0);
    EXPECT_NE(firstIdentifier, secondIdentifier);
    ASSERT_EQ(repository.all().size(), 2U);

    // Изменение входного объекта и полученных копий не должно менять хранилище.
    setTitle(item, "Изменённый исходный объект");
    auto found = repository.find(firstIdentifier);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(titleOf(*found), "Исходная запись");
    setTitle(*found, "Изменение через find");
    auto snapshot = repository.all();
    setTitle(snapshot.front(), "Изменение списка");
    const auto unchanged = repository.find(firstIdentifier);
    ASSERT_TRUE(unchanged.has_value());
    EXPECT_EQ(titleOf(*unchanged), "Исходная запись");

    EXPECT_TRUE(repository.update(*found));
    const auto updated = repository.find(firstIdentifier);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(titleOf(*updated), "Изменение через find");
    EXPECT_THROW(static_cast<void>(repository.add(*updated)), std::invalid_argument);

    EXPECT_TRUE(repository.remove(firstIdentifier));
    EXPECT_FALSE(repository.remove(firstIdentifier));
    EXPECT_FALSE(repository.find(firstIdentifier).has_value());
    EXPECT_FALSE(repository.update(*found));
    EXPECT_TRUE(repository.find(secondIdentifier).has_value());
    const auto thirdIdentifier = repository.add(item);
    EXPECT_NE(thirdIdentifier, firstIdentifier);
    EXPECT_NE(thirdIdentifier, secondIdentifier);
    EXPECT_EQ(repository.all().size(), 2U);
}

}  // namespace

namespace msa::core {

TEST(PlannerRepositoriesTest, TaskRepositoryContract)
{
    InMemoryTaskRepository memory;
    TaskRepository& repository = memory;
    checkRepositoryContract(repository, Task{.title = "Купить продукты"});
}

TEST(PlannerRepositoriesTest, EventRepositoryContract)
{
    InMemoryEventRepository memory;
    EventRepository& repository = memory;
    checkRepositoryContract(repository, Event{
        .title = "Тренировка",
        .date = std::chrono::year{2026} / std::chrono::November / 5,
        .repeat = EventRepeat::Weekly,
    });
}

TEST(PlannerRepositoriesTest, ReminderRepositoryContract)
{
    InMemoryReminderRepository memory;
    ReminderRepository& repository = memory;
    checkRepositoryContract(repository, Reminder{TimedReminder{.title = "Пить воду"}});
}

TEST(PlannerRepositoriesTest, HabitRepositoryContract)
{
    InMemoryHabitRepository memory;
    HabitRepository& repository = memory;
    checkRepositoryContract(repository, Habit{.name = "Чтение", .target = 20, .unit = "стр"});
}

TEST(PlannerRepositoriesTest, ReminderKindsShareIdentifiersAndKeepTheirData)
{
    InMemoryReminderRepository memory;
    ReminderRepository& repository = memory;
    const auto moment = std::chrono::local_days{
        std::chrono::year{2026} / std::chrono::November / 5} + std::chrono::hours{12};
    const auto timedIdentifier = repository.add(TimedReminder{
        .title = "Звонок", .dueAt = moment, .fired = true});
    const auto recurringIdentifier = repository.add(RecurringReminder{
        .title = "Разминка", .dueAt = moment, .interval = std::chrono::minutes{45}});
    const auto placeIdentifier = repository.add(PlaceReminder{
        .title = "Покупки", .place = "Магазин", .coordinates = GeoCoordinates{0, 0}});
    EXPECT_NE(timedIdentifier, recurringIdentifier);
    EXPECT_NE(timedIdentifier, placeIdentifier);
    EXPECT_NE(recurringIdentifier, placeIdentifier);

    const auto timed = repository.find(timedIdentifier);
    const auto recurring = repository.find(recurringIdentifier);
    const auto place = repository.find(placeIdentifier);
    ASSERT_TRUE(timed.has_value());
    ASSERT_TRUE(recurring.has_value());
    ASSERT_TRUE(place.has_value());
    ASSERT_TRUE(std::holds_alternative<TimedReminder>(*timed));
    ASSERT_TRUE(std::holds_alternative<RecurringReminder>(*recurring));
    ASSERT_TRUE(std::holds_alternative<PlaceReminder>(*place));
    EXPECT_EQ(std::get<TimedReminder>(*timed).identifier, timedIdentifier);
    EXPECT_EQ(std::get<TimedReminder>(*timed).dueAt, moment);
    EXPECT_TRUE(std::get<TimedReminder>(*timed).fired);
    EXPECT_EQ(std::get<RecurringReminder>(*recurring).interval, std::chrono::minutes{45});
    ASSERT_TRUE(std::get<PlaceReminder>(*place).coordinates.has_value());
    EXPECT_DOUBLE_EQ(std::get<PlaceReminder>(*place).coordinates->latitude, 0);
}

TEST(PlannerRepositoriesTest, RemovingHabitRemovesOnlyItsOwnLogs)
{
    InMemoryHabitRepository memory;
    HabitRepository& repository = memory;
    const auto firstIdentifier = repository.add(Habit{.name = "Чтение"});
    const auto secondIdentifier = repository.add(Habit{.name = "Ходьба"});
    const auto date = std::chrono::year{2026} / std::chrono::November / 5;
    EXPECT_FALSE(repository.addLog(HabitLogEntry{
        .habitIdentifier = kMissingIdentifier, .date = date, .value = 1}).has_value());
    EXPECT_TRUE(repository.logs(kMissingIdentifier).empty());
    const auto firstLogIdentifier = repository.addLog(HabitLogEntry{
        .habitIdentifier = firstIdentifier, .date = date, .value = 10});
    const auto secondLogIdentifier = repository.addLog(HabitLogEntry{
        .habitIdentifier = firstIdentifier, .date = date, .value = 5});
    const auto otherLogIdentifier = repository.addLog(HabitLogEntry{
        .habitIdentifier = secondIdentifier, .date = date, .value = 1000});
    ASSERT_TRUE(firstLogIdentifier.has_value());
    ASSERT_TRUE(secondLogIdentifier.has_value());
    ASSERT_TRUE(otherLogIdentifier.has_value());
    EXPECT_NE(*firstLogIdentifier, *secondLogIdentifier);

    auto entries = repository.logs(firstIdentifier);
    ASSERT_EQ(entries.size(), 2U);
    entries.front().value = 999;
    const auto unchanged = repository.logs(firstIdentifier);
    EXPECT_NE(unchanged.front().value, 999);
    EXPECT_TRUE(repository.remove(firstIdentifier));
    EXPECT_TRUE(repository.logs(firstIdentifier).empty());
    const auto remaining = repository.logs(secondIdentifier);
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ(remaining.front().identifier, *otherLogIdentifier);
    EXPECT_EQ(remaining.front().date, date);
    EXPECT_DOUBLE_EQ(remaining.front().value, 1000);
}

TEST(PlannerRepositoriesTest, ProfileIsReplacedAndReturnedAsCopy)
{
    InMemoryUserProfileRepository memory;
    UserProfileRepository& repository = memory;
    EXPECT_TRUE(repository.load().facts.empty());
    UserProfile profile{.facts = {"Меня зовут Настя", "Люблю читать"}};
    repository.save(profile);
    profile.facts.clear();
    auto loaded = repository.load();
    ASSERT_EQ(loaded.facts.size(), 2U);
    EXPECT_EQ(loaded.facts.front(), "Меня зовут Настя");
    loaded.facts.clear();
    EXPECT_EQ(repository.load().facts.size(), 2U);
    repository.save(UserProfile{.facts = {"Новый факт"}});
    ASSERT_EQ(repository.load().facts.size(), 1U);
    EXPECT_EQ(repository.load().facts.front(), "Новый факт");
    repository.save(UserProfile{});
    EXPECT_TRUE(repository.load().facts.empty());
}

TEST(PlannerRepositoriesTest, FakeClockCrossesMidnightWithoutWaiting)
{
    const auto start = std::chrono::local_days{
        std::chrono::year{2026} / std::chrono::December / 31} + std::chrono::hours{23}
        + std::chrono::minutes{59};
    FakeClock fake{start};
    const Clock& clock = fake;
    EXPECT_EQ(clock.now(), start);
    fake.advance(std::chrono::minutes{2});
    EXPECT_EQ(clock.now(), std::chrono::local_days{
        std::chrono::year{2027} / std::chrono::January / 1} + std::chrono::minutes{1});
    fake.set(start);
    EXPECT_EQ(clock.now(), start);
}

}  // namespace msa::core

