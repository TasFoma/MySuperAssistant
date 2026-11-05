#include "msa/core/HabitService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace msa::core {
namespace {

[[nodiscard]] bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] std::chrono::year_month_day todayOf(const Clock& clock)
{
    return std::chrono::year_month_day{std::chrono::sys_days{
        std::chrono::floor<std::chrono::days>(clock.now()).time_since_epoch()}};
}

}  // namespace

HabitService::HabitService(const Clock& clock, HabitRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
}

HabitWriteResult HabitService::addHabit(const std::string& name, double target,
                                      const std::string& unit, HabitPeriod period)
{
    if (!positiveFinite(target)) {
        return HabitRejection{"Цель должна быть положительным конечным числом."};
    }
    if (period != HabitPeriod::Day && period != HabitPeriod::Week) {
        return HabitRejection{"Выберите дневной или недельный период."};
    }

    return m_repository.add(Habit{.name = name, .target = target, .unit = unit, .period = period});
}

HabitWriteResult HabitService::logHabit(std::int64_t identifier, double value)
{
    if (!positiveFinite(value)) {
        return HabitRejection{"Отметка должна быть положительным конечным числом."};
    }
    const auto result = m_repository.addLog(HabitLogEntry{
        .habitIdentifier = identifier, .date = todayOf(m_clock), .value = value});
    if (!result) {
        return HabitRejection{"Привычка не найдена."};
    }

    return *result;
}

std::vector<HabitProgress> HabitService::habits() const
{
    const auto today = todayOf(m_clock);
    const auto todayDays = std::chrono::sys_days{today};
    const auto monday = std::chrono::year_month_day{todayDays - std::chrono::days{
        std::chrono::weekday{todayDays}.iso_encoding() - 1}};
    std::vector<HabitProgress> result;
    for (const auto& habit : m_repository.all()) {
        const auto start = habit.period == HabitPeriod::Week ? monday : today;
        double progress = 0.0;
        for (const auto& entry : m_repository.logs(habit.identifier)) {
            if (!entry.date.ok() || entry.date < start || entry.date > today) {
                continue;
            }
            if (!positiveFinite(entry.value)
                || progress > std::numeric_limits<double>::max() - entry.value) {
                throw std::runtime_error{"Некорректные данные прогресса привычки"};
            }
            progress += entry.value;
        }
        result.push_back(HabitProgress{habit, progress});
    }
    std::ranges::sort(result, [](const HabitProgress& left, const HabitProgress& right) {
        return left.habit.identifier < right.habit.identifier;
    });

    return result;
}

}  // namespace msa::core
