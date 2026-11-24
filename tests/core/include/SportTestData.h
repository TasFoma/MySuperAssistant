#pragma once

#include "msa/core/SportWorkoutService.h"

namespace msa::test
{
/// @brief Создаёт изолированное условное занятие для проверки правил, не контент приложения.
[[nodiscard]] core::SportSessionTemplate sportSession();
[[nodiscard]] core::SportWorkoutDraft sportDraft();
[[nodiscard]] core::SportWorkoutRevision sportResult();
} // namespace msa::test
