#pragma once

#include "msa/core/SportExercise.h"

namespace msa::infra
{
/// @brief Читает версионированный офлайн-пакет; неподготовленные карточки не опубликованы.
[[nodiscard]] std::vector<core::SportExerciseRevision> loadBundledSportCatalog();
} // namespace msa::infra
