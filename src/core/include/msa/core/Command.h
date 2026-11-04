#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Способ передвижения для маршрута.
enum class TravelMode : std::uint8_t {
    Foot,
    Driving,
    Bike,
};

/// @brief Команда не распознана.
struct NoCommand {};

/// @brief Напоминание по времени: «напомни завтра в 15:00 позвонить маме».
struct TimedReminderCommand {
    std::string title;
    std::chrono::local_time<std::chrono::minutes> dueAt;
};

/// @brief Напоминание по месту: «напомни купить хлеб, когда буду около магазина».
struct PlaceReminderCommand {
    std::string title;
    std::string place;
};

/// @brief Повторяющееся напоминание: «пить воду каждые 45 минут».
struct RecurringReminderCommand {
    std::string title;
    std::chrono::minutes interval{0};
};

/// @brief Запись в память: «запомни: меня зовут Настя».
struct RememberCommand {
    std::string fact;
};

/// @brief Список задач, пункты в порядке из текста.
struct TaskListCommand {
    std::vector<std::string> items;
};

/// @brief Одна задача с необязательным сроком: «добавь задачу купить продукты на завтра».
struct TaskCommand {
    std::string title;
    std::optional<std::chrono::year_month_day> dueDate;
};

/// @brief Событие календаря. Время — от полуночи, если указано.
struct EventCommand {
    std::string title;
    std::chrono::year_month_day date;
    std::optional<std::chrono::minutes> time;
};

/// @brief Маршрут: «как добраться до библиотеки на машине».
struct RouteCommand {
    std::string place;
    TravelMode mode{TravelMode::Foot};
};

/// @brief Результат разбора текста команды.
using Command = std::variant<NoCommand,
                             TimedReminderCommand,
                             PlaceReminderCommand,
                             RecurringReminderCommand,
                             RememberCommand,
                             TaskListCommand,
                             TaskCommand,
                             EventCommand,
                             RouteCommand>;

}  // namespace msa::core
