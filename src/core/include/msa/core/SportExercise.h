#pragma once

#include "msa/core/SportLoad.h"

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace msa::core
{
enum class SportSide
{
    Both,
    Left,
    Right,
    LeftArmRightLeg,
    RightArmLeftLeg
};
/// @brief Статические условия упражнения, не медицинский допуск.
struct SportExecutionRequirements
{
    std::optional<bool> palmSupport{};
    std::optional<bool> floorTransition{};
    std::optional<bool> kneeSupport{};
    std::optional<bool> stableSupport{};
    std::string supportType{};
};
/// @brief Фактические условия, необходимые для сопоставления результатов.
struct SportExecutionConditions
{
    std::optional<int> supportHeightMillimeters{};
    std::optional<int> wallDistanceMillimeters{};
    std::string amplitude{};
    std::string note{};
    bool comparable{false};
    auto operator<=>(const SportExecutionConditions&) const = default;
};
struct SportTechnique
{
    std::string brief{};
    std::string preparation{};
    std::vector<std::string> steps{};
    std::string breathing{};
    std::string counting{};
};
struct SportSource
{
    std::string title{};
    std::string url{};
    std::string checkedDate{};
};
/// @brief Неизменяемая редакция конкретного варианта упражнения.
struct SportExerciseRevision
{
    std::string identifier{};
    std::string revision{};
    std::string title{};
    std::vector<std::string> aliases{};
    std::vector<std::string> muscles{};
    std::vector<std::string> equipment{};
    std::vector<std::string> errors{};
    std::vector<std::string> limitations{};
    std::vector<std::string> alternatives{};
    SportTechnique technique{};
    SportExecutionRequirements requirements{};
    std::vector<SportSource> sources{};
    std::vector<SportSide> sides{SportSide::Both};
    int loadKind{0};
    bool published{false};
};
struct SportExercisePreference
{
    bool favorite{false};
    bool progression{false};
    bool operator==(const SportExercisePreference&) const = default;
};
/// @brief Проверяет полноту опубликованной карточки и идентичность варианта.
[[nodiscard]] std::optional<SportRejection> validateSportExercise(const SportExerciseRevision& exercise);
} // namespace msa::core
