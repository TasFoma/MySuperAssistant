#include "InMemoryEventRepository.h"

#include "msa/core/CalendarService.h"
#include "msa/core/EventService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

using std::chrono::minutes;
using std::chrono::year;

TEST(EventServiceTest, CreatesSingleCharacterPastAndUntimedEventsWithDefaults)
{
    InMemoryEventRepository repository;
    EventService service{repository};
    const auto identifier = std::get<std::int64_t>(service.addEvent("Я", year{2000} / 1 / 1, std::nullopt));
    const auto event = repository.find(identifier);
    ASSERT_TRUE(event);
    EXPECT_GT(identifier, 0);
    EXPECT_EQ(event->identifier, identifier);
    EXPECT_EQ(event->title, "Я");
    EXPECT_EQ(event->date, year{2000} / 1 / 1);
    EXPECT_FALSE(event->time);
    EXPECT_TRUE(event->description.empty());
    EXPECT_EQ(event->repeat, EventRepeat::None);
}

TEST(EventServiceTest, AcceptsTimeBoundariesAndAllRepeatModes)
{
    InMemoryEventRepository repository;
    EventService service{repository};
    for (const auto repeat : {EventRepeat::None, EventRepeat::Daily,
                              EventRepeat::Weekly, EventRepeat::Monthly}) {
        for (const auto time : {minutes{0}, minutes{1439}}) {
            const auto identifier = std::get<std::int64_t>(service.addEvent(
                "A", year{2024} / 2 / 29, time, "Описание", repeat));
            const auto event = repository.find(identifier);
            ASSERT_TRUE(event);
            EXPECT_EQ(event->time, time);
            EXPECT_EQ(event->description, "Описание");
            EXPECT_EQ(event->repeat, repeat);
        }
    }
    EXPECT_EQ(repository.all().size(), 8U);
}

TEST(EventServiceTest, RejectsInvalidCreateAndUpdateWithoutChangingStorage)
{
    InMemoryEventRepository repository;
    EventService service{repository};
    const auto date = year{2026} / 11 / 5;
    const auto identifier = std::get<std::int64_t>(service.addEvent(
        "Исходное", date, minutes{60}, "Описание", EventRepeat::Weekly));
    const std::vector<Event> invalid{
        Event{.title = "", .date = date},
        Event{.title = "Я", .date = year{2026} / 2 / 29},
        Event{.title = "Я", .date = year{2026} / 13 / 1},
        Event{.title = "Я", .date = date, .time = minutes{-1}},
        Event{.title = "Я", .date = date, .time = minutes{1440}},
        Event{.title = "Я", .date = date, .repeat = static_cast<EventRepeat>(255)}};
    for (const auto& input : invalid) {
        const auto result = service.addEvent(input.title, input.date, input.time,
                                             input.description, input.repeat);
        ASSERT_TRUE(std::holds_alternative<EventRejection>(result));
        EXPECT_FALSE(std::get<EventRejection>(result).reason.empty());
        const auto changed = service.updateEvent(identifier, input.title, input.date,
            input.time, input.description, input.repeat);
        ASSERT_TRUE(std::holds_alternative<EventRejection>(changed));
        EXPECT_FALSE(std::get<EventRejection>(changed).reason.empty());
        const auto saved = repository.find(identifier);
        ASSERT_TRUE(saved);
        EXPECT_EQ(saved->title, "Исходное");
        EXPECT_EQ(saved->date, date);
        EXPECT_EQ(saved->time, minutes{60});
        EXPECT_EQ(saved->description, "Описание");
        EXPECT_EQ(saved->repeat, EventRepeat::Weekly);
    }
    EXPECT_EQ(repository.all().size(), 1U);
}

TEST(EventServiceTest, UpdatesEntireSeriesAndCanClearOptionalFields)
{
    InMemoryEventRepository repository;
    EventService service{repository};
    CalendarService calendar{repository};
    const auto identifier = std::get<std::int64_t>(service.addEvent(
        "Старое", year{2026} / 1 / 1, minutes{60}, "Описание", EventRepeat::Daily));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.updateEvent(identifier,
        "Новое", year{2026} / 1 / 31, minutes{1439}, "Новое описание", EventRepeat::Monthly)));
    const auto event = repository.find(identifier);
    ASSERT_TRUE(event);
    EXPECT_EQ(event->identifier, identifier);
    EXPECT_EQ(event->title, "Новое");
    EXPECT_EQ(event->date, year{2026} / 1 / 31);
    EXPECT_EQ(event->time, minutes{1439});
    EXPECT_EQ(event->description, "Новое описание");
    EXPECT_EQ(event->repeat, EventRepeat::Monthly);
    EXPECT_TRUE(calendar.eventsForDate(year{2026} / 1 / 1).empty());
    EXPECT_TRUE(calendar.eventsForDate(year{2026} / 2 / 1).empty());
    const auto february = calendar.eventsForDate(year{2026} / 2 / 28);
    ASSERT_EQ(february.size(), 1U);
    EXPECT_EQ(february.front().identifier, identifier);
    EXPECT_EQ(february.front().title, "Новое");
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.updateEvent(identifier,
        "Я", year{2000} / 1 / 1, std::nullopt, "", EventRepeat::None)));
    EXPECT_FALSE(repository.find(identifier)->time);
    EXPECT_TRUE(repository.find(identifier)->description.empty());
    EXPECT_EQ(repository.find(identifier)->repeat, EventRepeat::None);
    EXPECT_TRUE(calendar.eventsForDate(year{2026} / 2 / 28).empty());
    EXPECT_EQ(calendar.eventsForDate(year{2000} / 1 / 1).size(), 1U);
    EXPECT_EQ(repository.all().size(), 1U);
}

TEST(EventServiceTest, MissingEventReturnsReasonWithoutCreatingRecord)
{
    InMemoryEventRepository repository;
    EventService service{repository};
    const auto result = service.updateEvent(42, "Нет", year{2026} / 11 / 5,
        std::nullopt, "", EventRepeat::None);
    ASSERT_TRUE(std::holds_alternative<EventRejection>(result));
    EXPECT_FALSE(std::get<EventRejection>(result).reason.empty());
    EXPECT_TRUE(repository.all().empty());
}

}  // namespace
}  // namespace msa::core

