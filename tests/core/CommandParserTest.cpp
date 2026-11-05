// Тесты типов команд, временных границ и свободных форм.
#include "msa/core/CommandParser.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <variant>

namespace {

using std::chrono::day;
using std::chrono::hours;
using std::chrono::local_days;
using std::chrono::local_time;
using std::chrono::minutes;
using std::chrono::month;
using std::chrono::seconds;
using std::chrono::sys_days;
using std::chrono::year;
using std::chrono::year_month_day;

constexpr int kTestYear = 2026;
constexpr unsigned kOctober = 10;
constexpr unsigned kTuesdayDay = 27;
constexpr unsigned kTomorrowDay = 28;
constexpr unsigned kFridayDay = 30;
constexpr int kNoonHour = 12;
constexpr int kEveningHour = 15;
constexpr int kEighteenHour = 18;

/// @brief Дата 2026-10-DD.
[[nodiscard]] year_month_day dateOf(unsigned dayOfMonth)
{
    return year_month_day{year{kTestYear}, month{kOctober}, day{dayOfMonth}};
}

/// @brief Момент времени: день и час по местному времени.
[[nodiscard]] local_time<minutes> localAt(year_month_day date, hours hour)
{
    return local_days{sys_days{date}.time_since_epoch()} + hour;
}

/// @brief Вторник 2026-10-27, 12:00 — опорное время для большинства тестов.
[[nodiscard]] local_time<seconds> tuesdayNoon()
{
    return std::chrono::time_point_cast<seconds>(localAt(dateOf(kTuesdayDay), hours{kNoonHour}));
}

}  // namespace

namespace msa::core {

TEST(CommandParserTest, ParsesTimedReminder)
{
    const CommandParser parser;

    const Command command = parser.parse("напомни завтра в 15:00 позвонить маме", tuesdayNoon());

    const auto* reminder = std::get_if<TimedReminderCommand>(&command);
    ASSERT_NE(reminder, nullptr);
    EXPECT_EQ(reminder->title, "Позвонить маме");
    EXPECT_EQ(reminder->dueAt, localAt(dateOf(kTomorrowDay), hours{kEveningHour}));
}

TEST(CommandParserTest, ParsesPlaceReminder)
{
    const CommandParser parser;

    const Command command =
        parser.parse("напомни купить хлеб, когда буду около магазина", tuesdayNoon());

    const auto* reminder = std::get_if<PlaceReminderCommand>(&command);
    ASSERT_NE(reminder, nullptr);
    EXPECT_EQ(reminder->title, "Купить хлеб");
    EXPECT_EQ(reminder->place, "Магазина");
}

TEST(CommandParserTest, ParsesRecurringReminder)
{
    const CommandParser parser;

    const Command command = parser.parse("пить воду каждые 45 минут", tuesdayNoon());

    const auto* reminder = std::get_if<RecurringReminderCommand>(&command);
    ASSERT_NE(reminder, nullptr);
    EXPECT_EQ(reminder->title, "Пить воду");
    EXPECT_EQ(reminder->interval, minutes{45});
}

TEST(CommandParserTest, ParsesRemember)
{
    const CommandParser parser;

    const Command command = parser.parse("запомни: меня зовут Настя", tuesdayNoon());

    const auto* remember = std::get_if<RememberCommand>(&command);
    ASSERT_NE(remember, nullptr);
    EXPECT_EQ(remember->fact, "меня зовут Настя");
}

TEST(CommandParserTest, ParsesTaskList)
{
    const CommandParser parser;

    const Command command = parser.parse(
        "вот задачи, которые надо сделать: 1. Купить молоко 2. Позвонить маме 3. Написать отчёт",
        tuesdayNoon());

    const auto* list = std::get_if<TaskListCommand>(&command);
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->items.size(), 3U);
    EXPECT_EQ(list->items[0], "Купить молоко");
    EXPECT_EQ(list->items[2], "Написать отчёт");
}

TEST(CommandParserTest, ParsesTask)
{
    const CommandParser parser;

    const Command command = parser.parse("добавь задачу купить продукты на завтра", tuesdayNoon());

    const auto* task = std::get_if<TaskCommand>(&command);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->title, "Купить продукты");
    ASSERT_TRUE(task->dueDate.has_value());
    EXPECT_EQ(*task->dueDate, dateOf(kTomorrowDay));
}

TEST(CommandParserTest, ParsesWeekdayAsTodayWhenTodayIsThatWeekday)
{
    const CommandParser parser;
    const auto friday = std::chrono::time_point_cast<seconds>(
        localAt(dateOf(kFridayDay), hours{kNoonHour}));

    const Command command = parser.parse("добавь задачу купить подарок в пятницу", friday);

    const auto* task = std::get_if<TaskCommand>(&command);
    ASSERT_NE(task, nullptr);
    ASSERT_TRUE(task->dueDate.has_value());
    EXPECT_EQ(*task->dueDate, dateOf(kFridayDay));
}

TEST(CommandParserTest, ParsesEvent)
{
    const CommandParser parser;

    const Command command =
        parser.parse("добавь встречу с Иваном в пятницу в 18:00", tuesdayNoon());

    const auto* event = std::get_if<EventCommand>(&command);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->title, "Встреча с Иваном");
    EXPECT_EQ(event->date, dateOf(kFridayDay));
    ASSERT_TRUE(event->time.has_value());
    EXPECT_EQ(*event->time, minutes{kEighteenHour * 60});
}

TEST(CommandParserTest, ParsesRoute)
{
    const CommandParser parser;

    const Command command =
        parser.parse("как добраться до библиотеки на машине", tuesdayNoon());

    const auto* route = std::get_if<RouteCommand>(&command);
    ASSERT_NE(route, nullptr);
    EXPECT_EQ(route->place, "Библиотеки");
    EXPECT_EQ(route->mode, TravelMode::Driving);
}

TEST(CommandParserTest, ReturnsNoCommandForUnrecognizedText)
{
    const CommandParser parser;

    const Command command = parser.parse("как дела", tuesdayNoon());

    EXPECT_TRUE(std::holds_alternative<NoCommand>(command));
}

TEST(CommandParserTest, ParsesRelativeReminderIntervals)
{
    struct Example {
        std::string_view text;
        minutes delay;
    };
    const std::array examples{
        Example{"напомни через полчаса пить воду", minutes{30}},
        Example{"напомни через минуту пить воду", minutes{1}},
        Example{"напомни через 45 минут пить воду", minutes{45}},
        Example{"напомни через пять минут пить воду", minutes{5}},
        Example{"напомни через пару часов пить воду", minutes{120}},
        Example{"напомни через час пить воду", minutes{60}},
        Example{"напомни через 2 дня пить воду", minutes{2880}},
    };
    const CommandParser parser;
    for (const auto& example : examples) {
        SCOPED_TRACE(example.text);
        const auto command = parser.parse(example.text, tuesdayNoon());
        const auto* reminder = std::get_if<TimedReminderCommand>(&command);
        ASSERT_NE(reminder, nullptr);
        EXPECT_EQ(reminder->title, "Пить воду");
        EXPECT_EQ(reminder->dueAt,
                  localAt(dateOf(kTuesdayDay), hours{kNoonHour}) + example.delay);
    }
}

TEST(CommandParserTest, RelativeReminderCrossesMonthBoundary)
{
    const CommandParser parser;
    const auto now = localAt(dateOf(31), hours{23}) + minutes{45};
    const auto command = parser.parse("напомни через полчаса пить воду", now);
    const auto* reminder = std::get_if<TimedReminderCommand>(&command);
    ASSERT_NE(reminder, nullptr);
    EXPECT_EQ(reminder->title, "Пить воду");
    EXPECT_EQ(reminder->dueAt,
              localAt(year{2026} / month{11} / day{1}, hours{0}) + minutes{15});
}

TEST(CommandParserTest, PastOrCurrentReminderTimeMovesToTomorrow)
{
    const CommandParser parser;
    for (const auto text : {"напомни в 11:00 пить воду", "напомни в 12:00 пить воду"}) {
        SCOPED_TRACE(text);
        const auto command = parser.parse(text, tuesdayNoon());
        const auto* reminder = std::get_if<TimedReminderCommand>(&command);
        ASSERT_NE(reminder, nullptr);
        EXPECT_EQ(reminder->title, "Пить воду");
        EXPECT_EQ(std::chrono::floor<std::chrono::days>(reminder->dueAt),
                  local_days{sys_days{dateOf(kTomorrowDay)}.time_since_epoch()});
        EXPECT_EQ(reminder->dueAt.time_since_epoch() % std::chrono::days{1},
                  text == std::string_view{"напомни в 11:00 пить воду"} ? hours{11} : hours{12});
    }
}

TEST(CommandParserTest, DefaultReminderCrossesMidnight)
{
    const CommandParser parser;
    const auto now = localAt(dateOf(kTuesdayDay), hours{23}) + minutes{45};
    const auto command = parser.parse("напомни пить воду", now);
    const auto* reminder = std::get_if<TimedReminderCommand>(&command);
    ASSERT_NE(reminder, nullptr);
    EXPECT_EQ(reminder->title, "Пить воду");
    EXPECT_EQ(reminder->dueAt, localAt(dateOf(kTomorrowDay), hours{0}) + minutes{45});
}

TEST(CommandParserTest, ParsesFreeTasks)
{
    struct Example {
        std::string_view text;
        std::optional<year_month_day> dueDate;
    };
    const std::array examples{
        Example{"прочитать книгу на завтра, добавь в задачи", dateOf(kTomorrowDay)},
        Example{"добавь пожалуйста в список задач: «прочитать книгу»", std::nullopt},
        Example{"у меня появилась новая задача прочитать книгу, добавь пожалуйста", std::nullopt},
        Example{"сегодня мне надо прочитать книгу, добавь в задачи", dateOf(kTuesdayDay)},
    };
    const CommandParser parser;
    for (const auto& example : examples) {
        SCOPED_TRACE(example.text);
        const auto command = parser.parse(example.text, tuesdayNoon());
        const auto* task = std::get_if<TaskCommand>(&command);
        ASSERT_NE(task, nullptr);
        EXPECT_EQ(task->title, "Прочитать книгу");
        EXPECT_EQ(task->dueDate, example.dueDate);
    }
}

TEST(CommandParserTest, ParsesFreeEvents)
{
    struct Example {
        std::string_view text;
        year_month_day date;
        std::optional<minutes> time;
    };
    const std::array examples{
        Example{"завтра тренировка в 18:00, добавь в календарь", dateOf(kTomorrowDay), minutes{1080}},
        Example{"у меня будет тренировка, запиши в календарь", dateOf(kTuesdayDay), std::nullopt},
        Example{"создай пожалуйста событие «Тренировка» на завтра", dateOf(kTomorrowDay), std::nullopt},
    };
    const CommandParser parser;
    for (const auto& example : examples) {
        SCOPED_TRACE(example.text);
        const auto command = parser.parse(example.text, tuesdayNoon());
        const auto* event = std::get_if<EventCommand>(&command);
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->title, "Тренировка");
        EXPECT_EQ(event->date, example.date);
        EXPECT_EQ(event->time, example.time);
    }
}

TEST(CommandParserTest, RejectsFreeFormsWithTooShortTitles)
{
    const CommandParser parser;
    for (const auto text : {"добавь в задачи «Я»", "Я, добавь в календарь"}) {
        SCOPED_TRACE(text);
        EXPECT_TRUE(std::holds_alternative<NoCommand>(parser.parse(text, tuesdayNoon())));
    }
}

}  // namespace msa::core
