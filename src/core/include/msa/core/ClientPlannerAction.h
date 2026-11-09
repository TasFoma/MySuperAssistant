#pragma once

#include "msa/core/Event.h"
#include "msa/core/Habit.h"
#include "msa/core/Task.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace msa::core
{
/// @brief Создание или изменение задачи; done и createdAt не передаются на изменение.
struct ClientSaveTask
{
    std::optional<std::int64_t> identifier{};
    std::string title{};
    std::optional<std::chrono::year_month_day> dueDate{};
    TaskPriority priority{TaskPriority::Normal};
    bool operator==(const ClientSaveTask&) const = default;
};
/// @brief Создание или изменение исходной серии события, не отдельного вхождения.
struct ClientSaveEvent
{
    std::optional<std::int64_t> identifier{};
    std::string title{};
    std::chrono::year_month_day date{};
    std::optional<std::chrono::minutes> time{};
    std::string description{};
    EventRepeat repeat{EventRepeat::None};
    bool operator==(const ClientSaveEvent&) const = default;
};
/// @brief Создание привычки с количественной целью.
struct ClientCreateHabit
{
    std::string name{};
    double target{};
    std::string unit{};
    HabitPeriod period{HabitPeriod::Day};
    bool operator==(const ClientCreateHabit&) const = default;
};
/// @brief Положительная отметка привычки; дату определяет сервер.
struct ClientLogHabit
{
    std::int64_t identifier{};
    double value{};
    bool operator==(const ClientLogHabit&) const = default;
};
/// @brief Виды записей, удаляемых после явного подтверждения.
enum class ClientItemKind
{
    Task,
    Event,
    Habit,
    Reminder
};
/// @brief Удаление записи по точному предметному ID.
struct ClientDeleteItem
{
    ClientItemKind kind{ClientItemKind::Task};
    std::int64_t identifier{};
    bool operator==(const ClientDeleteItem&) const = default;
};
/// @brief Напоминание в местный момент серверной зоны.
struct ClientCreateTimedReminder
{
    std::string title{};
    std::chrono::local_seconds dueAt{};
    bool operator==(const ClientCreateTimedReminder&) const = default;
};
/// @brief Повторяющееся напоминание; первый срок вычисляет сервер.
struct ClientCreateRecurringReminder
{
    std::string title{};
    std::chrono::minutes interval{};
    bool operator==(const ClientCreateRecurringReminder&) const = default;
};
/// @brief Напоминание по названию места без выдуманных координат.
struct ClientCreatePlaceReminder
{
    std::string title{};
    std::string place{};
    bool operator==(const ClientCreatePlaceReminder&) const = default;
};
/// @brief Действия форм планировщика без Qt, JSON и сетевого поведения.
using ClientPlannerAction =
    std::variant<ClientSaveTask, ClientSaveEvent, ClientCreateHabit, ClientLogHabit, ClientDeleteItem,
                 ClientCreateTimedReminder, ClientCreateRecurringReminder, ClientCreatePlaceReminder>;
} // namespace msa::core
