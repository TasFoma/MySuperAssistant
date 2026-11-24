#include "msa/core/SportWorkoutService.h"

#include <algorithm>
#include <set>

namespace msa::core
{
namespace
{
constexpr int kMaximumRepetitions = 1000;
constexpr std::size_t kMaximumSets = 100;
constexpr auto kMaximumDuration = std::chrono::hours{24 * 7};
[[nodiscard]] SportWorkoutItem makeItem(const SportProgramItem& plan)
{
    SportWorkoutItem item;
    item.plan = plan;
    for (const auto& planned : plan.sets)
    {
        item.actual.push_back({planned.identifier,
                               planned.identifier,
                               planned.side,
                               SportSetState::Pending,
                               std::nullopt,
                               planned.load,
                               {}});
    }

    return item;
}
[[nodiscard]] std::optional<SportRejection> validateActual(const SportActualSet& actual,
                                                           const SportExerciseRevision& exercise)
{
    if (actual.identifier.empty() || std::ranges::find(exercise.sides, actual.side) == exercise.sides.end() ||
        static_cast<int>(actual.load.index()) != exercise.loadKind)
    {
        return SportRejection{"set", "Проверьте сторону и режим нагрузки."};
    }
    if (const auto* dumbbells = std::get_if<DumbbellLoad>(&actual.load);
        dumbbells && exercise.requirements.dumbbellQuantity &&
        dumbbells->quantity != *exercise.requirements.dumbbellQuantity)
    {
        return SportRejection{"equipment", "Количество гантелей не соответствует карточке упражнения."};
    }
    if (actual.state == SportSetState::Performed)
    {
        if (!actual.repetitions || *actual.repetitions < 1 || *actual.repetitions > kMaximumRepetitions)
        {
            return SportRejection{"repetitions", "Выполненный подход: от 1 до 1000 повторений."};
        }
        return validateSportLoad(actual.load, true);
    }
    if (actual.state != SportSetState::Pending && actual.state != SportSetState::Skipped)
    {
        return SportRejection{"set", "Некорректное состояние подхода."};
    }
    if (actual.repetitions)
    {
        return SportRejection{"set", "Пропуск и незаполненный подход не содержат выполненных повторений."};
    }

    return validateSportLoad(actual.load);
}
[[nodiscard]] SportResult<SportWorkoutRevision> prepare(const SportWorkoutDraft& draft, const std::string& revision)
{
    if (draft.identifier.empty() || revision.empty() || !draft.date.ok() || draft.items.empty())
    {
        return SportRejection{"workout", "Укажите идентичность, дату и состав тренировки."};
    }
    if (const auto error = validateSportSession(draft.originalPlan))
    {
        return *error;
    }
    if (const auto error = validateSportWellbeing(draft.before))
    {
        return *error;
    }
    if (const auto error = validateSportWellbeing(draft.after))
    {
        return *error;
    }
    if (summarizeSportDuration(draft.timer).unresolved)
    {
        return SportRejection{"interval", "Сначала учтите время отсутствия."};
    }
    if (draft.timer.manualDuration &&
        (*draft.timer.manualDuration <= SportDuration::zero() || *draft.timer.manualDuration > kMaximumDuration))
    {
        return SportRejection{"time", "Проверьте ручную длительность."};
    }
    bool performed{};
    std::set<std::string> identifiers;
    for (const auto& item : draft.items)
    {
        if (!identifiers.insert(item.plan.identifier).second || item.actual.empty() ||
            item.actual.size() > kMaximumSets)
        {
            return SportRejection{"item", "Проверьте пункты и количество подходов."};
        }
        if (const auto error = validateSportSession({"validation", "Проверка", {item.plan}}))
        {
            return *error;
        }
        std::set<std::string> sets;
        for (const auto& actual : item.actual)
        {
            if (!sets.insert(actual.identifier).second)
            {
                return SportRejection{"set", "Повторная идентичность подхода."};
            }
            if (const auto error = validateActual(actual, item.plan.exercise))
            {
                return *error;
            }
            if (actual.state == SportSetState::Pending)
            {
                return SportRejection{"pending", "Отметьте незаполненные подходы или их пропуск."};
            }
            performed = performed || actual.state == SportSetState::Performed;
        }
    }

    return SportWorkoutRevision{revision, {}, draft, !performed};
}
} // namespace
SportResult<SportWorkoutDraft> SportWorkoutService::start(const std::optional<SportWorkoutDraft>& active,
                                                          const SportSessionTemplate& session,
                                                          const std::string& identifier,
                                                          std::chrono::year_month_day date, SportInstant instant) const
{
    if (active)
    {
        return SportRejection{"active", "На устройстве уже есть незавершённая тренировка."};
    }
    if (identifier.empty() || !date.ok())
    {
        return SportRejection{"workout", "Проверьте идентичность и дату начала."};
    }
    if (const auto error = validateSportSession(session))
    {
        return *error;
    }
    SportWorkoutDraft draft;
    draft.identifier = identifier;
    draft.date = date;
    draft.originalPlan = session;
    draft.timer = startSportTimer(instant);
    for (const auto& item : session.items)
    {
        draft.items.push_back(makeItem(item));
    }

    return draft;
}
SportResult<SportWorkoutDraft> SportWorkoutService::recordSet(const SportWorkoutDraft& draft,
                                                              const std::string& itemIdentifier,
                                                              const SportActualSet& actual) const
{
    auto result = draft;
    const auto item =
        std::ranges::find(result.items, itemIdentifier, [](const auto& value) { return value.plan.identifier; });
    if (item == result.items.end())
    {
        return SportRejection{"item", "Упражнение не найдено."};
    }
    if (actual.state != SportSetState::Performed)
    {
        return SportRejection{"set", "Ожидается явная отметка выполнения."};
    }
    if (const auto error = validateActual(actual, item->plan.exercise))
    {
        return *error;
    }
    const auto target = std::ranges::find(item->actual, actual.identifier, &SportActualSet::identifier);
    if (target == item->actual.end())
    {
        return SportRejection{"set", "Подход не найден."};
    }
    auto replacement = actual;
    replacement.plannedIdentifier = target->plannedIdentifier;
    *target = replacement;

    return result;
}
SportResult<SportWorkoutDraft> SportWorkoutService::addSet(const SportWorkoutDraft& draft,
                                                           const std::string& itemIdentifier,
                                                           const SportActualSet& actual) const
{
    auto result = draft;
    const auto item =
        std::ranges::find(result.items, itemIdentifier, [](const auto& value) { return value.plan.identifier; });
    if (item == result.items.end() || item->actual.size() >= kMaximumSets)
    {
        return SportRejection{"item", "Упражнение не найдено или достигнут предел подходов."};
    }
    if (std::ranges::find(item->actual, actual.identifier, &SportActualSet::identifier) != item->actual.end())
    {
        return SportRejection{"set", "Подход уже существует."};
    }
    if (const auto error = validateActual(actual, item->plan.exercise))
    {
        return *error;
    }
    auto added = actual;
    added.plannedIdentifier.clear();
    item->actual.push_back(added);

    return result;
}
SportResult<SportWorkoutDraft> SportWorkoutService::skipSet(const SportWorkoutDraft& draft,
                                                            const std::string& itemIdentifier,
                                                            const std::string& setIdentifier) const
{
    auto result = draft;
    const auto item =
        std::ranges::find(result.items, itemIdentifier, [](const auto& value) { return value.plan.identifier; });
    if (item == result.items.end())
    {
        return SportRejection{"item", "Упражнение не найдено."};
    }
    const auto target = std::ranges::find(item->actual, setIdentifier, &SportActualSet::identifier);
    if (target == item->actual.end())
    {
        return SportRejection{"set", "Подход не найден."};
    }
    target->state = SportSetState::Skipped;
    target->repetitions.reset();

    return result;
}
SportResult<SportWorkoutDraft> SportWorkoutService::skipExercise(const SportWorkoutDraft& draft,
                                                                 const std::string& itemIdentifier) const
{
    auto result = draft;
    const auto item =
        std::ranges::find(result.items, itemIdentifier, [](const auto& value) { return value.plan.identifier; });
    if (item == result.items.end())
    {
        return SportRejection{"item", "Упражнение не найдено."};
    }
    for (auto& actual : item->actual)
    {
        if (actual.state == SportSetState::Pending)
        {
            actual.state = SportSetState::Skipped;
            actual.repetitions.reset();
        }
    }

    return result;
}
SportResult<SportWorkoutDraft> SportWorkoutService::replaceExercise(const SportWorkoutDraft& draft,
                                                                    const std::string& itemIdentifier,
                                                                    const SportProgramItem& replacement) const
{
    if (std::ranges::any_of(draft.items,
                            [replacement](const auto& item) { return item.plan.identifier == replacement.identifier; }))
    {
        return SportRejection{"item", "Замена должна иметь собственную идентичность пункта."};
    }
    if (const auto error = validateSportSession({"replacement", "Замена", {replacement}}))
    {
        return *error;
    }
    const auto skipped = skipExercise(draft, itemIdentifier);
    if (const auto* error = std::get_if<SportRejection>(&skipped))
    {
        return *error;
    }
    auto result = std::get<SportWorkoutDraft>(skipped);
    auto item = makeItem(replacement);
    item.replacesItem = itemIdentifier;
    const auto original =
        std::ranges::find(result.items, itemIdentifier, [](const auto& value) { return value.plan.identifier; });
    result.items.insert(original + 1, item);

    return result;
}
SportResult<SportWorkoutRevision> SportWorkoutService::finish(const SportWorkoutDraft& draft,
                                                              const std::string& revisionIdentifier) const
{
    return prepare(draft, revisionIdentifier);
}
SportResult<SportWorkoutRevision> SportWorkoutService::correct(const SportWorkoutRevision& current,
                                                               const std::string& expectedRevision,
                                                               const SportWorkoutDraft& edited,
                                                               const std::string& revisionIdentifier) const
{
    if (current.identifier != expectedRevision || revisionIdentifier.empty() ||
        revisionIdentifier == current.identifier || edited.identifier != current.workout.identifier)
    {
        return SportRejection{"revision", "Результат изменился или указана другая тренировка."};
    }
    auto corrected = edited;
    corrected.originalPlan = current.workout.originalPlan;
    corrected.programIdentifier = current.workout.programIdentifier;
    corrected.programRevision = current.workout.programRevision;
    corrected.utcOffsetMinutes = current.workout.utcOffsetMinutes;
    corrected.restrictionsSnapshot = current.workout.restrictionsSnapshot;
    corrected.timer = current.workout.timer;
    corrected.timer.manualDuration = edited.timer.manualDuration;
    auto result = prepare(corrected, revisionIdentifier);
    if (auto* revision = std::get_if<SportWorkoutRevision>(&result))
    {
        revision->parentRevision = current.identifier;
    }

    return result;
}
} // namespace msa::core
