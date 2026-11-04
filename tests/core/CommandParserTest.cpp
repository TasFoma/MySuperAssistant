// Тесты разбора команд: по одному на каждый тип команды и на нераспознанный текст.
#include "msa/core/CommandParser.h"

#include <gtest/gtest.h>

#include <chrono>
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

}  // namespace msa::core
