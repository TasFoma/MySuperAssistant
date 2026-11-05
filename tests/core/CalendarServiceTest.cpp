#include "InMemoryEventRepository.h"
#include "msa/core/CalendarService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

namespace msa::core {
namespace {

using std::chrono::day;
using std::chrono::minutes;
using std::chrono::year;

TEST(CalendarServiceTest, HandlesOriginalDailyAndWeeklyDatesWithoutEarlierOccurrences)
{
    for (const auto repeat : {EventRepeat::None, EventRepeat::Daily, EventRepeat::Weekly}) {
        InMemoryEventRepository repository;
        const auto start = year{2026} / 12 / 31;
        const auto identifier = repository.add(Event{.title = "Встреча", .date = start, .repeat = repeat});
        CalendarService calendar{repository};
        EXPECT_TRUE(calendar.eventsForDate(year{2026} / 12 / 30).empty());
        const auto initial = calendar.eventsForDate(start);
        ASSERT_EQ(initial.size(), 1U);
        EXPECT_EQ(initial.front().identifier, identifier);
        EXPECT_EQ(calendar.eventsForDate(year{2027} / 1 / 1).size(), repeat == EventRepeat::Daily ? 1U : 0U);
        EXPECT_EQ(calendar.eventsForDate(year{2027} / 1 / 7).size(), repeat == EventRepeat::None ? 0U : 1U);
    }
}

TEST(CalendarServiceTest, MonthlyClampsToMonthEndAndReturnsToOriginalDay)
{
    InMemoryEventRepository repository;
    const auto start = year{2024} / 1 / 31;
    const auto identifier = repository.add(Event{.title = "Платёж", .date = start, .repeat = EventRepeat::Monthly});
    CalendarService calendar{repository};
    for (const auto date : {year{2024} / 2 / 29, year{2024} / 3 / 31,
                            year{2024} / 4 / 30, year{2025} / 2 / 28}) {
        const auto events = calendar.eventsForDate(date);
        ASSERT_EQ(events.size(), 1U);
        EXPECT_EQ(events.front().date, start);
    }
    EXPECT_TRUE(calendar.eventsForDate(year{2024} / 3 / 30).empty());
    EXPECT_EQ(repository.find(identifier)->date, start);
}

TEST(CalendarServiceTest, LeapDayMonthlyEventUsesFebruaryEndInNonLeapYear)
{
    InMemoryEventRepository repository;
    static_cast<void>(repository.add(Event{.title = "Встреча", .date = year{2024} / 2 / 29,
                                         .repeat = EventRepeat::Monthly}));
    CalendarService calendar{repository};
    EXPECT_EQ(calendar.eventsForDate(year{2025} / 2 / 28).size(), 1U);
    EXPECT_EQ(calendar.eventsForDate(year{2025} / 3 / 29).size(), 1U);
    EXPECT_TRUE(calendar.eventsForDate(year{2025} / 3 / 31).empty());
}

TEST(CalendarServiceTest, SortsWithoutTimeThenByTimeAndIdentifierAndReturnsCopies)
{
    InMemoryEventRepository repository;
    const auto date = year{2026} / 11 / 5;
    const auto later = repository.add(Event{.title = "Вечер", .date = date, .time = minutes{1080}});
    const auto first = repository.add(Event{.title = "Полночь", .date = date, .time = minutes{0}});
    const auto second = repository.add(Event{.title = "Ещё полночь", .date = date, .time = minutes{0}});
    const auto untimed = repository.add(Event{.title = "День", .date = date});
    CalendarService calendar{repository};
    auto events = calendar.eventsForDate(date);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events[0].identifier, untimed);
    EXPECT_EQ(events[1].identifier, first);
    EXPECT_EQ(events[2].identifier, second);
    EXPECT_EQ(events[3].identifier, later);
    events[0].title = "Изменено";
    EXPECT_EQ(repository.find(untimed)->title, "День");
}

TEST(CalendarServiceTest, MonthDaysAreUniqueSortedAndIncludeClampedOccurrences)
{
    InMemoryEventRepository repository;
    static_cast<void>(repository.add(Event{.title = "Повтор", .date = year{2024} / 1 / 31,
                                         .repeat = EventRepeat::Monthly}));
    static_cast<void>(repository.add(Event{.title = "Дубликат", .date = year{2025} / 2 / 28}));
    static_cast<void>(repository.add(Event{.title = "Раньше", .date = year{2025} / 2 / 2}));
    CalendarService calendar{repository};
    EXPECT_EQ(calendar.eventDaysInMonth(year{2025} / 2), (std::vector<day>{day{2}, day{28}}));
    EXPECT_TRUE(calendar.eventDaysInMonth(year{2023} / 12).empty());
}

TEST(CalendarServiceTest, InvalidDatesAndEmptyRepositoryProduceEmptyResults)
{
    InMemoryEventRepository repository;
    CalendarService calendar{repository};
    EXPECT_TRUE(calendar.eventsForDate(year{2025} / 2 / 28).empty());
    EXPECT_TRUE(calendar.eventDaysInMonth(year{2025} / 2).empty());
    static_cast<void>(repository.add(Event{.title = "Неверно", .date = year{2025} / 2 / 30,
                                         .repeat = EventRepeat::Daily}));
    EXPECT_TRUE(calendar.eventsForDate(year{2025} / 3 / 1).empty());
    EXPECT_TRUE(calendar.eventsForDate(year{2025} / 2 / 30).empty());
    EXPECT_TRUE(calendar.eventDaysInMonth(year{2025} / 13).empty());
}

TEST(CalendarServiceTest, MonthDaysIncludeDailyAndWeeklyRepeatsOnlyFromSeriesStart)
{
    InMemoryEventRepository repository;
    const auto weekly = repository.add(Event{
        .date = year{2026} / 11 / 23, .repeat = EventRepeat::Weekly});
    CalendarService calendar{repository};
    EXPECT_EQ(calendar.eventDaysInMonth(year{2026} / 11),
        (std::vector<day>{day{23}, day{30}}));
    EXPECT_EQ(calendar.eventDaysInMonth(year{2026} / 12),
        (std::vector<day>{day{7}, day{14}, day{21}, day{28}}));
    static_cast<void>(repository.add(Event{
        .date = year{2026} / 11 / 29, .repeat = EventRepeat::Daily}));
    EXPECT_EQ(calendar.eventDaysInMonth(year{2026} / 11),
        (std::vector<day>{day{23}, day{29}, day{30}}));
    EXPECT_EQ(repository.find(weekly)->date, year{2026} / 11 / 23);
}

}  // namespace
}  // namespace msa::core
