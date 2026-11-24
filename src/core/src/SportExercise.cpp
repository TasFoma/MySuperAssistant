#include "msa/core/SportExercise.h"

#include <algorithm>
#include <set>

namespace msa::core
{
std::optional<SportRejection> validateSportExercise(const SportExerciseRevision& exercise)
{
    if (exercise.identifier.empty() || exercise.revision.empty() || exercise.title.empty() || exercise.loadKind < 0 ||
        exercise.loadKind > 2 || exercise.sides.empty())
    {
        return SportRejection{"exercise", "Укажите вариант упражнения и редакцию."};
    }
    std::set<SportSide> sides;
    for (const auto side : exercise.sides)
    {
        if (side < SportSide::Both || side > SportSide::RightArmLeftLeg || !sides.insert(side).second)
        {
            return SportRejection{"side", "Некорректные стороны упражнения."};
        }
    }
    if (!exercise.published)
    {
        return std::nullopt;
    }
    if (exercise.technique.brief.empty() || exercise.technique.preparation.empty() ||
        exercise.technique.steps.empty() || exercise.technique.counting.empty() ||
        exercise.technique.breathing.empty() || exercise.muscles.empty() || exercise.errors.empty() ||
        exercise.limitations.empty() || exercise.sources.empty() || !exercise.requirements.palmSupport ||
        !exercise.requirements.floorTransition || !exercise.requirements.kneeSupport ||
        !exercise.requirements.stableSupport)
    {
        return SportRejection{"content", "Опубликованная карточка не заполнена полностью."};
    }
    const auto emptyText = [](const std::string& text) { return text.empty(); };
    if (std::ranges::any_of(exercise.technique.steps, emptyText) || std::ranges::any_of(exercise.muscles, emptyText) ||
        std::ranges::any_of(exercise.errors, emptyText) || std::ranges::any_of(exercise.limitations, emptyText))
    {
        return SportRejection{"content", "В карточке есть пустые обязательные сведения."};
    }
    for (const auto& source : exercise.sources)
    {
        if (source.title.empty() || !source.url.starts_with("https://") || source.checkedDate.empty())
        {
            return SportRejection{"source", "Источник ещё не проверен."};
        }
    }

    return std::nullopt;
}
} // namespace msa::core
