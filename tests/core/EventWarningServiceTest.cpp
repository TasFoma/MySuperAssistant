#include "FakeClock.h"
#include "InMemoryEventRepository.h"
#include "InMemoryEventWarningRepository.h"

#include "msa/core/EventWarningService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

namespace msa::core {
namespace {

using std::chrono::days;
using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::year;

TEST(EventWarningServiceTest, TimedWindowIncludesExactlyHourAndLastSecondButNotStartOrPast)
{
    const auto date = year{2026} / 11 / 6;
    const auto start = local_days{date} + hours{12};
    // Отдельное состояние для каждой границы: предыдущая выдача не маскирует результат.
    for (const auto offset : {seconds{-3601}, seconds{-3600}, seconds{-1}, seconds{0}, seconds{1}}) {
        FakeClock clock{start + offset};
        InMemoryEventRepository events;
        InMemoryEventWarningRepository marks;
        CalendarService calendar{events};
        EventWarningService service{clock, calendar, marks};
        const auto identifier = events.add(Event{.title = "Встреча", .date = date, .time = hours{12}});
        const auto due = service.checkDue();
        const bool expected = offset >= -hours{1} && offset < seconds::zero();
        ASSERT_EQ(due.size(), expected ? 1U : 0U);
        if (expected) {
            EXPECT_EQ(due.front().event.identifier, identifier);
            EXPECT_EQ(due.front().occurrenceDate, date);
            EXPECT_EQ(due.front().kind, EventWarningKind::BeforeStart);
        }
    }
}

TEST(EventWarningServiceTest, CrossesMidnightAndYearWithoutDuplicateAfterMidnight)
{
    const auto date = year{2026} / 12 / 31;
    FakeClock clock{local_days{date} + hours{23} + minutes{50}};
    InMemoryEventRepository events;
    InMemoryEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    const auto identifier = events.add(Event{.date = year{2027} / 1 / 1, .time = minutes{10}});
    const auto due = service.checkDue();
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().event.identifier, identifier);
    EXPECT_EQ(due.front().occurrenceDate, year{2027} / 1 / 1);
    clock.advance(minutes{11});
    EXPECT_TRUE(service.checkDue().empty());
}

TEST(EventWarningServiceTest, UntimedIssuesBothKindsFromEightAndKeepsThemAcrossServiceRecreation)
{
    const auto date = year{2026} / 11 / 6;
    FakeClock clock{local_days{date} + hours{8} - seconds{1}};
    InMemoryEventRepository events;
    InMemoryEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    const auto identifier = events.add(Event{.title = "Праздник", .date = year{2026} / 11 / 7});
    EXPECT_TRUE(service.checkDue().empty());
    clock.advance(seconds{1});
    auto due = service.checkDue();
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().kind, EventWarningKind::DayBefore);
    EXPECT_EQ(due.front().occurrenceDate, year{2026} / 11 / 7);
    EXPECT_TRUE(service.checkDue().empty());
    EventWarningService recreated{clock, calendar, marks};
    EXPECT_TRUE(recreated.checkDue().empty());
    clock.set(local_days{year{2026} / 11 / 7} + hours{8} - seconds{1});
    EXPECT_TRUE(recreated.checkDue().empty());
    clock.advance(seconds{1});
    due = recreated.checkDue();
    ASSERT_EQ(due.size(), 1U);
    EXPECT_EQ(due.front().event.identifier, identifier);
    EXPECT_EQ(due.front().kind, EventWarningKind::OnDay);
    EXPECT_TRUE(recreated.checkDue().empty());
    clock.advance(days{1});
    EXPECT_TRUE(recreated.checkDue().empty());
}

TEST(EventWarningServiceTest, LateStartupIssuesTodayAndTomorrowButDoesNotReplayPastDays)
{
    const auto date = year{2026} / 11 / 6;
    FakeClock clock{local_days{date} + hours{23}};
    InMemoryEventRepository events;
    InMemoryEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    static_cast<void>(events.add(Event{.date = year{2026} / 11 / 5}));
    const auto today = events.add(Event{.date = date});
    const auto tomorrow = events.add(Event{.date = year{2026} / 11 / 7});
    static_cast<void>(events.add(Event{.date = year{2026} / 11 / 8}));
    const auto due = service.checkDue();
    ASSERT_EQ(due.size(), 2U);
    EXPECT_EQ(due[0].event.identifier, today);
    EXPECT_EQ(due[0].kind, EventWarningKind::OnDay);
    EXPECT_EQ(due[1].event.identifier, tomorrow);
    EXPECT_EQ(due[1].kind, EventWarningKind::DayBefore);
}

TEST(EventWarningServiceTest, TimedMarkSurvivesTimeChangeAndServiceRecreation)
{
    const auto date = year{2026} / 11 / 6;
    FakeClock clock{local_days{date} + hours{11}};
    InMemoryEventRepository events;
    InMemoryEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    const auto identifier = events.add(Event{.date = date, .time = hours{12}});
    ASSERT_EQ(service.checkDue().size(), 1U);
    auto event = *events.find(identifier);
    event.time = hours{13};
    ASSERT_TRUE(events.update(event));
    clock.advance(hours{1});
    EventWarningService recreated{clock, calendar, marks};
    EXPECT_TRUE(recreated.checkDue().empty());
}

TEST(EventWarningServiceTest, RepeatsGetIndependentOccurrenceDatesAndMonthlyClamp)
{
    for (const auto repeat : {EventRepeat::Daily, EventRepeat::Weekly, EventRepeat::Monthly}) {
        FakeClock clock{local_days{year{2026} / 1 / 31} + hours{11}};
        InMemoryEventRepository events;
        InMemoryEventWarningRepository marks;
        CalendarService calendar{events};
        EventWarningService service{clock, calendar, marks};
        static_cast<void>(events.add(Event{.date = year{2026} / 1 / 31,
            .time = hours{12}, .repeat = repeat}));
        ASSERT_EQ(service.checkDue().size(), 1U);
        const auto next = repeat == EventRepeat::Daily ? year{2026} / 2 / 1
            : repeat == EventRepeat::Weekly ? year{2026} / 2 / 7 : year{2026} / 2 / 28;
        clock.set(local_days{next} + hours{11});
        const auto due = service.checkDue();
        ASSERT_EQ(due.size(), 1U);
        EXPECT_EQ(due.front().occurrenceDate, next);
        EXPECT_EQ(due.front().event.date, year{2026} / 1 / 31);
        EXPECT_TRUE(service.checkDue().empty());
    }
}

TEST(EventWarningServiceTest, UntimedDailyRepeatSeparatesKindsAndOccurrences)
{
    FakeClock clock{local_days{year{2026} / 11 / 6} + hours{8}};
    InMemoryEventRepository events;
    InMemoryEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    static_cast<void>(events.add(Event{.date = year{2026} / 11 / 6, .repeat = EventRepeat::Daily}));
    ASSERT_EQ(service.checkDue().size(), 2U);
    clock.advance(days{1});
    const auto due = service.checkDue();
    ASSERT_EQ(due.size(), 2U);
    EXPECT_EQ(due[0].occurrenceDate, year{2026} / 11 / 7);
    EXPECT_EQ(due[0].kind, EventWarningKind::OnDay);
    EXPECT_EQ(due[1].occurrenceDate, year{2026} / 11 / 8);
    EXPECT_EQ(due[1].kind, EventWarningKind::DayBefore);
}

TEST(EventWarningServiceTest, RepositorySeparatesIdentifierDateAndKind)
{
    InMemoryEventWarningRepository marks;
    const auto date = year{2026} / 11 / 6;
    EXPECT_TRUE(marks.markIfNew(1, date, EventWarningKind::OnDay));
    EXPECT_FALSE(marks.markIfNew(1, date, EventWarningKind::OnDay));
    EXPECT_TRUE(marks.markIfNew(2, date, EventWarningKind::OnDay));
    EXPECT_TRUE(marks.markIfNew(1, year{2026} / 11 / 7, EventWarningKind::OnDay));
    EXPECT_TRUE(marks.markIfNew(1, date, EventWarningKind::DayBefore));
    EXPECT_TRUE(marks.markIfNew(1, date, EventWarningKind::BeforeStart));
}

/// @brief Имитирует техническую ошибку сохранения отметки.
class FailingEventWarningRepository final : public EventWarningRepository {
public:
    [[nodiscard]] bool markIfNew([[maybe_unused]] std::int64_t identifier,
        [[maybe_unused]] std::chrono::year_month_day occurrenceDate,
        [[maybe_unused]] EventWarningKind kind) override;
};

bool FailingEventWarningRepository::markIfNew([[maybe_unused]] std::int64_t identifier,
    [[maybe_unused]] std::chrono::year_month_day occurrenceDate,
    [[maybe_unused]] EventWarningKind kind)
{
    throw std::runtime_error{"Ошибка сохранения отметки"};
}

TEST(EventWarningServiceTest, PropagatesMarkFailureAndDoesNotChangeEvents)
{
    const auto date = year{2026} / 11 / 6;
    FakeClock clock{local_days{date} + hours{11}};
    InMemoryEventRepository events;
    FailingEventWarningRepository marks;
    CalendarService calendar{events};
    EventWarningService service{clock, calendar, marks};
    const auto identifier = events.add(Event{.date = date, .time = hours{12}});
    EXPECT_THROW(static_cast<void>(service.checkDue()), std::runtime_error);
    EXPECT_EQ(events.find(identifier)->time, hours{12});
}

}  // namespace
}  // namespace msa::core

