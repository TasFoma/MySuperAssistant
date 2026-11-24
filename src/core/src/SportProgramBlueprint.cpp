#include "msa/core/SportProgramBlueprint.h"

#include <algorithm>
#include <set>
#include <utility>

namespace msa::core
{
namespace
{
[[nodiscard]] const SportExerciseRevision* findExercise(std::span<const SportExerciseRevision> catalog,
                                                        const std::string& identifier)
{
    const auto found = std::ranges::find(catalog, identifier, &SportExerciseRevision::identifier);
    if (found == catalog.end())
    {
        return nullptr;
    }

    return &*found;
}
[[nodiscard]] bool incomplete(const std::vector<std::string>& values)
{
    return values.empty() || std::ranges::any_of(values, [](const auto& value) { return value.empty(); });
}
} // namespace
std::optional<SportRejection> validateSportProgramBlueprint(const SportProgramBlueprint& blueprint,
                                                            std::span<const SportExerciseRevision> catalog)
{
    if (blueprint.identifier.empty() || blueprint.revision.empty() || blueprint.code.empty() ||
        blueprint.title.empty() || blueprint.description.empty() || blueprint.authorship.empty() ||
        blueprint.sessions.empty() || incomplete(blueprint.equipment) || incomplete(blueprint.conditions))
    {
        return SportRejection{"blueprint", "Не заполнено описание заготовки программы."};
    }
    std::set<std::string> sessions;
    std::set<std::string> items;
    for (const auto& session : blueprint.sessions)
    {
        if (session.identifier.empty() || !sessions.insert(session.identifier).second || session.title.empty() ||
            session.description.empty() || session.items.empty())
        {
            return SportRejection{"blueprint", "Проверьте занятия заготовки программы."};
        }
        for (const auto& item : session.items)
        {
            if (item.identifier.empty() || !items.insert(item.identifier).second || item.instructions.empty() ||
                !findExercise(catalog, item.exerciseIdentifier))
            {
                return SportRejection{"blueprint", "Пункт заготовки ссылается на неизвестное упражнение."};
            }
            std::set<std::string> alternatives;
            for (const auto& alternative : item.alternatives)
            {
                if (alternative.exerciseIdentifier == item.exerciseIdentifier || alternative.description.empty() ||
                    !alternatives.insert(alternative.exerciseIdentifier).second ||
                    !findExercise(catalog, alternative.exerciseIdentifier))
                {
                    return SportRejection{"blueprint", "Проверьте ручные альтернативы заготовки программы."};
                }
            }
        }
    }

    return std::nullopt;
}
SportResult<SportProgramRevision> makeSportProgramBlueprintCopy(const SportProgramBlueprint& blueprint,
                                                                std::span<const SportExerciseRevision> catalog,
                                                                const std::string& identifier,
                                                                const std::string& revision)
{
    if (const auto error = validateSportProgramBlueprint(blueprint, catalog))
    {
        return *error;
    }
    if (identifier.empty() || revision.empty() || identifier == blueprint.identifier || revision == blueprint.revision)
    {
        return SportRejection{"identity", "Копии нужна собственная идентичность и редакция."};
    }
    SportProgramRevision program;
    program.identifier = identifier;
    program.revision = revision;
    program.sourceRevision = blueprint.revision;
    program.title = blueprint.title + " — копия";
    for (const auto& source : blueprint.sessions)
    {
        SportSessionTemplate session;
        session.identifier = identifier + ":" + source.identifier;
        session.title = source.title;
        for (const auto& sourceItem : source.items)
        {
            const auto* exercise = findExercise(catalog, sourceItem.exerciseIdentifier);
            if (!exercise || !exercise->published)
            {
                return SportRejection{"unpublished",
                                      "Карточка упражнения в составе ещё не готова. Копирование недоступно."};
            }
            if (const auto error = validateSportExercise(*exercise))
            {
                return *error;
            }
            SportProgramItem item;
            item.identifier = identifier + ":" + sourceItem.identifier;
            item.exercise = *exercise;
            // Условия выполнения и дозировку задаёт пользователь. Редакционные пояснения не становятся условиями
            // прогресса.
            session.items.push_back(std::move(item));
        }
        program.sessions.push_back(std::move(session));
    }

    return program;
}
} // namespace msa::core
