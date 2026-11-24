#pragma once

#include "msa/core/SportWorkout.h"

#include <compare>
#include <span>

namespace msa::core
{
struct SportProgressKey
{
    std::string exerciseIdentifier{};
    SportSide side{SportSide::Both};
    int loadKind{0};
    int dumbbellQuantity{0};
    int bodyweightMode{0};
    SportExecutionConditions conditions{};
    std::string unknownContext{};
    auto operator<=>(const SportProgressKey&) const = default;
};
struct SportExerciseResult
{
    std::string workoutIdentifier{};
    std::string revisionIdentifier{};
    std::chrono::year_month_day date{};
    SportActualSet actual{};
};
struct SportProgressSeries
{
    SportProgressKey key{};
    std::vector<SportExerciseResult> results{};
};
/// @brief Проецирует текущие сохранённые редакции; конфликтующие входы отклоняются.
[[nodiscard]] SportResult<std::vector<SportProgressSeries>>
buildSportProgress(std::span<const SportWorkoutRecord> records);
} // namespace msa::core
