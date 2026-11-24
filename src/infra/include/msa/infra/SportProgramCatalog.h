#pragma once

#include "msa/core/SportProgramBlueprint.h"

#include <span>
#include <vector>

namespace msa::infra
{
/// @brief Читает офлайн-заготовки P1/P3, проверяя версию, структуру и ссылки на каталог.
/// @return Авторские составы без подходов, повторений, веса или автоматической прогрессии.
[[nodiscard]] std::vector<core::SportProgramBlueprint>
loadBundledSportProgramCatalog(std::span<const core::SportExerciseRevision> catalog);
} // namespace msa::infra
