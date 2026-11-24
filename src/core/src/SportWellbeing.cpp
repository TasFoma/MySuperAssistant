#include "msa/core/SportWellbeing.h"

#include <algorithm>
#include <set>

namespace msa::core
{
std::optional<SportRejection> validateSportWellbeing(const SportWellbeing& value)
{
    const auto valid = [](const std::optional<int>& number, int minimum, int maximum)
    { return !number || (*number >= minimum && *number <= maximum); };
    if (!valid(value.wellbeing, 1, 5) || !valid(value.fatigue, 1, 5) || !valid(value.pain, 0, 10))
    {
        return SportRejection{"wellbeing", "Оценки: самочувствие и усталость 1–5, боль 0–10."};
    }
    std::set<std::string> areas;
    for (const auto& area : value.areas)
    {
        if (area.empty() || !areas.insert(area).second)
        {
            return SportRejection{"area", "Проверьте области боли."};
        }
    }
    if (areas.contains("other") && value.otherArea.empty())
    {
        return SportRejection{"area", "Опишите другую область."};
    }

    return std::nullopt;
}
} // namespace msa::core
