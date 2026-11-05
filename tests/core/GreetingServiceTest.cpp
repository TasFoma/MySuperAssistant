#include "FakeClock.h"
#include "InMemoryEventRepository.h"
#include "InMemoryGreetingRepository.h"
#include "InMemoryTaskRepository.h"
#include "msa/core/GreetingService.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

namespace msa::core {
namespace {

using std::chrono::year;

TEST(GreetingServiceTest, ChoosesSalutationAtTimeBoundaries)
{
    struct Example {
        int minute;
        std::string_view prefix;
    };
    const std::array examples{
        Example{0, "Доброй ночи"}, Example{479, "Доброй ночи"},
        Example{480, "Доброе утро"}, Example{719, "Доброе утро"},
        Example{720, "Добрый день"}, Example{1079, "Добрый день"},
        Example{1080, "Добрый вечер"}, Example{1439, "Добрый вечер"}};
    for (const auto& example : examples) {
        SCOPED_TRACE(example.minute);
        FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}
                        + std::chrono::minutes{example.minute}};
        InMemoryEventRepository events;
        InMemoryTaskRepository tasks;
        InMemoryGreetingRepository greetings;
        CalendarService calendar{events};
        GreetingService service{clock, calendar, tasks, greetings};
        const auto message = service.greetingIfNeeded();
        ASSERT_TRUE(message);
        EXPECT_TRUE(message->starts_with(example.prefix));
        EXPECT_NE(message->find("четверг, 5 ноября 2026"), std::string::npos);
        EXPECT_NE(message->find("Планов пока нет"), std::string::npos);
    }
}

TEST(GreetingServiceTest, SharedRepositoryPreventsRepeatAfterServiceRecreationAndAllowsNextDay)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 12 / 31} + std::chrono::hours{23}};
    InMemoryEventRepository events;
    InMemoryTaskRepository tasks;
    InMemoryGreetingRepository greetings;
    CalendarService calendar{events};
    GreetingService first{clock, calendar, tasks, greetings};
    EXPECT_FALSE(greetings.lastDate());
    EXPECT_TRUE(first.greetingIfNeeded());
    EXPECT_FALSE(first.greetingIfNeeded());
    GreetingService restarted{clock, calendar, tasks, greetings};
    EXPECT_FALSE(restarted.greetingIfNeeded());
    clock.advance(std::chrono::hours{1});
    const auto message = restarted.greetingIfNeeded();
    ASSERT_TRUE(message);
    EXPECT_TRUE(message->starts_with("Доброй ночи"));
    EXPECT_EQ(greetings.lastDate(), year{2027} / 1 / 1);
}

TEST(GreetingServiceTest, IncludesTodaysRecurringEventsWithTimeAndExcludesOtherDates)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryEventRepository events;
    InMemoryTaskRepository tasks;
    InMemoryGreetingRepository greetings;
    CalendarService calendar{events};
    GreetingService service{clock, calendar, tasks, greetings};
    static_cast<void>(events.add(Event{.title = "Разминка", .date = year{2026} / 11 / 4,
                                       .time = std::chrono::minutes{545}, .repeat = EventRepeat::Daily}));
    static_cast<void>(events.add(Event{.title = "Выходной", .date = year{2026} / 11 / 5}));
    static_cast<void>(events.add(Event{.title = "Завтра", .date = year{2026} / 11 / 6}));
    const auto message = service.greetingIfNeeded();
    ASSERT_TRUE(message);
    EXPECT_NE(message->find("09:05 — Разминка"), std::string::npos);
    EXPECT_LT(message->find("Выходной"), message->find("Разминка"));
    EXPECT_EQ(message->find("Завтра"), std::string::npos);
    EXPECT_EQ(message->find("Планов пока нет"), std::string::npos);
}

TEST(GreetingServiceTest, SelectsFiveOpenTasksByPriorityDateThenIdentifier)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryEventRepository events;
    InMemoryTaskRepository tasks;
    InMemoryGreetingRepository greetings;
    CalendarService calendar{events};
    GreetingService service{clock, calendar, tasks, greetings};
    static_cast<void>(tasks.add(Task{.title = "Готово", .priority = TaskPriority::High, .done = true}));
    static_cast<void>(tasks.add(Task{.title = "Низкий", .priority = TaskPriority::Low}));
    static_cast<void>(tasks.add(Task{.title = "Без срока", .priority = TaskPriority::High}));
    static_cast<void>(tasks.add(Task{.title = "Позже", .dueDate = year{2026} / 11 / 7, .priority = TaskPriority::High}));
    static_cast<void>(tasks.add(Task{.title = "Первый", .dueDate = year{2026} / 11 / 5, .priority = TaskPriority::High}));
    static_cast<void>(tasks.add(Task{.title = "Второй", .dueDate = year{2026} / 11 / 5, .priority = TaskPriority::High}));
    static_cast<void>(tasks.add(Task{.title = "Обычный", .dueDate = year{2026} / 11 / 4}));
    const auto message = service.greetingIfNeeded();
    ASSERT_TRUE(message);
    EXPECT_LT(message->find("Первый"), message->find("Второй"));
    EXPECT_LT(message->find("Второй"), message->find("Позже"));
    EXPECT_LT(message->find("Позже"), message->find("Без срока"));
    EXPECT_LT(message->find("Без срока"), message->find("Обычный"));
    EXPECT_NE(message->find("Обычный"), std::string::npos);
    EXPECT_EQ(message->find("Низкий"), std::string::npos);
    EXPECT_EQ(message->find("Готово"), std::string::npos);
}

class FailingGreetingRepository final : public GreetingRepository {
public:
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override
    {
        return std::nullopt;
    }
    void saveDate([[maybe_unused]] std::chrono::year_month_day date) override
    {
        throw std::runtime_error{"Ошибка сохранения"};
    }
};

TEST(GreetingServiceTest, DoesNotReturnGreetingWhenSavingDateFails)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryEventRepository events;
    InMemoryTaskRepository tasks;
    FailingGreetingRepository greetings;
    CalendarService calendar{events};
    GreetingService service{clock, calendar, tasks, greetings};
    EXPECT_THROW(static_cast<void>(service.greetingIfNeeded()), std::runtime_error);
}

}  // namespace
}  // namespace msa::core
