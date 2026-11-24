#pragma once

#include "msa/core/SportRejection.h"

#include <optional>
#include <string>
#include <vector>

namespace msa::core
{
/// @brief Независимые необязательные показатели, не алгоритм назначения нагрузки.
struct SportWellbeing
{
    std::optional<int> wellbeing{};
    std::optional<int> fatigue{};
    std::optional<int> pain{};
    std::vector<std::string> areas{};
    std::string otherArea{};
    std::optional<bool> newPain{};
    std::string comment{};
};
/// @brief Проверяет диапазоны и текст другой области.
[[nodiscard]] std::optional<SportRejection> validateSportWellbeing(const SportWellbeing& value);
} // namespace msa::core
