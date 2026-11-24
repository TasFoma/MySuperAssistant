#pragma once

#include "msa/core/SportProgram.h"
#include "msa/core/SportTimer.h"
#include "msa/core/SportWellbeing.h"

#include <chrono>

namespace msa::core
{
enum class SportSetState
{
    Pending,
    Performed,
    Skipped
};
struct SportActualSet
{
    std::string identifier{};
    std::string plannedIdentifier{};
    SportSide side{SportSide::Both};
    SportSetState state{SportSetState::Pending};
    std::optional<int> repetitions{};
    SportLoad load{};
    std::string skipReason{};
};
struct SportWorkoutItem
{
    SportProgramItem plan{};
    std::vector<SportActualSet> actual{};
    std::string replacesItem{};
};
/// @brief Снимок активного занятия; черновик не объявляется сохранённым в core.
struct SportWorkoutDraft
{
    std::string identifier{};
    std::string programIdentifier{};
    std::string programRevision{};
    std::chrono::year_month_day date{};
    int utcOffsetMinutes{0};
    SportSessionTemplate originalPlan{};
    std::vector<SportWorkoutItem> items{};
    SportTimerState timer{};
    SportWellbeing before{};
    SportWellbeing after{};
    std::string notes{};
};
/// @brief Кандидат редакции результата. Успех подготовки ещё не подтверждает commit.
struct SportWorkoutRevision
{
    std::string identifier{};
    std::string parentRevision{};
    SportWorkoutDraft workout{};
    bool entirelySkipped{false};
};
struct SportWorkoutRecord
{
    SportWorkoutRevision current{};
};
} // namespace msa::core
