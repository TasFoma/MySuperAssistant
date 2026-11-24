#pragma once

#include "msa/core/SportProgram.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace msa::core
{
/// @brief Ручная альтернатива с отличиями условий, без эквивалентного веса.
struct SportProgramBlueprintAlternative
{
    std::string exerciseIdentifier{};
    std::string description{};
};
/// @brief Пункт авторской заготовки; подходы, повторения и вес отсутствуют.
struct SportProgramBlueprintItem
{
    std::string identifier{};
    std::string exerciseIdentifier{};
    std::string instructions{};
    std::vector<SportProgramBlueprintAlternative> alternatives{};
};
/// @brief Упорядоченный состав занятия, который ещё не является готовым планом.
struct SportProgramBlueprintSession
{
    std::string identifier{};
    std::string title{};
    std::string description{};
    std::vector<SportProgramBlueprintItem> items{};
};
/// @brief Версионированный состав P1/P3 без назначенной дозировки и медицинского допуска.
struct SportProgramBlueprint
{
    std::string identifier{};
    std::string revision{};
    std::string code{};
    std::string title{};
    std::string description{};
    std::string authorship{};
    std::vector<std::string> equipment{};
    std::vector<std::string> conditions{};
    std::vector<SportProgramBlueprintSession> sessions{};
};
/// @brief Проверяет состав и существование ссылок; неопубликованный пункт ещё можно просматривать.
[[nodiscard]] std::optional<SportRejection>
validateSportProgramBlueprint(const SportProgramBlueprint& blueprint, std::span<const SportExerciseRevision> catalog);
/// @brief Создаёт независимый неполный план из опубликованных снимков, оставляя все списки подходов пустыми.
/// @param identifier Новая идентичность пользовательской программы, назначенная apps.
/// @param revision Новая редакция копии; происхождение сохраняется отдельно.
/// @return Неполная собственная программа либо отказ при неготовом контенте/идентичностях.
[[nodiscard]] SportResult<SportProgramRevision>
makeSportProgramBlueprintCopy(const SportProgramBlueprint& blueprint, std::span<const SportExerciseRevision> catalog,
                              const std::string& identifier, const std::string& revision);
} // namespace msa::core
