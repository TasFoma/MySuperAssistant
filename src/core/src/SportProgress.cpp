#include "msa/core/SportProgress.h"

#include <algorithm>
#include <map>
#include <set>

namespace msa::core
{
SportResult<std::vector<SportProgressSeries>> buildSportProgress(std::span<const SportWorkoutRecord> records)
{
    std::map<SportProgressKey, std::vector<SportExerciseResult>> grouped;
    std::set<std::string> identifiers;
    for (const auto& record : records)
    {
        const auto& revision = record.current;
        const auto& workout = revision.workout;
        if (revision.identifier.empty() || workout.identifier.empty() || !identifiers.insert(workout.identifier).second)
        {
            return SportRejection{"duplicate", "Передайте одну текущую редакцию каждой тренировки."};
        }
        if (revision.entirelySkipped)
        {
            continue;
        }
        for (const auto& item : workout.items)
        {
            for (const auto& actual : item.actual)
            {
                if (actual.state != SportSetState::Performed)
                {
                    continue;
                }
                if (!actual.repetitions || *actual.repetitions < 1 || *actual.repetitions > 1000)
                {
                    return SportRejection{"set", "Некорректный выполненный подход."};
                }
                if (const auto error = validateSportLoad(actual.load, true))
                {
                    return *error;
                }
                SportProgressKey key;
                key.exerciseIdentifier = item.plan.exercise.identifier;
                key.side = actual.side;
                key.loadKind = static_cast<int>(actual.load.index());
                key.conditions = item.plan.conditions;
                if (!key.conditions.comparable)
                {
                    key.unknownContext =
                        std::to_string(workout.identifier.size()) + ":" + workout.identifier + item.plan.identifier;
                }
                if (const auto* load = std::get_if<DumbbellLoad>(&actual.load))
                {
                    key.dumbbellQuantity = load->quantity;
                }
                if (const auto* load = std::get_if<BodyweightLoad>(&actual.load))
                {
                    key.bodyweightMode = load->extra ? 1 : (load->assistance ? 2 : 0);
                }
                grouped[key].push_back({workout.identifier, revision.identifier, workout.date, actual});
            }
        }
    }
    std::vector<SportProgressSeries> result;
    for (auto& [key, values] : grouped)
    {
        std::ranges::stable_sort(values,
                                 [](const auto& first, const auto& second) { return first.date < second.date; });
        result.push_back({key, std::move(values)});
    }

    return result;
}
} // namespace msa::core
