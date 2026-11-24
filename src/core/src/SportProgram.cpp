#include "msa/core/SportProgram.h"

#include <algorithm>
#include <set>

namespace msa::core
{
namespace
{
constexpr std::size_t kMaximumSets = 100;
constexpr int kMaximumRepetitions = 1000;
} // namespace
std::optional<SportRejection> validateSportSession(const SportSessionTemplate& session)
{
    if (session.identifier.empty() || session.title.empty() || session.items.empty())
    {
        return SportRejection{"session", "Заполните занятие и добавьте упражнения."};
    }
    std::set<std::string> identifiers;
    for (const auto& item : session.items)
    {
        if (item.identifier.empty() || !identifiers.insert(item.identifier).second || item.sets.empty() ||
            item.sets.size() > kMaximumSets)
        {
            return SportRejection{"item", "Проверьте пункты и количество подходов."};
        }
        if (!item.exercise.published)
        {
            return SportRejection{"unpublished", "Карточка упражнения ещё не подготовлена к тренировке."};
        }
        if (const auto error = validateSportExercise(item.exercise))
        {
            return error;
        }
        if (item.conditions.supportHeightMillimeters && *item.conditions.supportHeightMillimeters <= 0)
        {
            return SportRejection{"conditions", "Высота опоры должна быть положительной."};
        }
        std::set<std::string> sets;
        for (const auto& planned : item.sets)
        {
            if (planned.identifier.empty() || !sets.insert(planned.identifier).second || planned.repetitions < 1 ||
                planned.repetitions > kMaximumRepetitions ||
                static_cast<int>(planned.load.index()) != item.exercise.loadKind ||
                std::ranges::find(item.exercise.sides, planned.side) == item.exercise.sides.end())
            {
                return SportRejection{"set", "Проверьте повторения, сторону и режим подхода."};
            }
            if (const auto* dumbbells = std::get_if<DumbbellLoad>(&planned.load);
                dumbbells && item.exercise.requirements.dumbbellQuantity &&
                dumbbells->quantity != *item.exercise.requirements.dumbbellQuantity)
            {
                return SportRejection{"equipment", "Количество гантелей не соответствует карточке упражнения."};
            }
            if (const auto error = validateSportLoad(planned.load))
            {
                return error;
            }
        }
    }

    return std::nullopt;
}
std::optional<SportRejection> validateSportProgram(const SportProgramRevision& program)
{
    if (program.identifier.empty() || program.revision.empty() || program.title.empty() || program.sessions.empty())
    {
        return SportRejection{"program", "Заполните программу и добавьте занятие."};
    }
    std::set<std::string> identifiers;
    for (const auto& session : program.sessions)
    {
        if (!identifiers.insert(session.identifier).second)
        {
            return SportRejection{"session", "Занятия должны иметь разные идентичности."};
        }
        if (const auto error = validateSportSession(session))
        {
            return error;
        }
    }

    return std::nullopt;
}
} // namespace msa::core
