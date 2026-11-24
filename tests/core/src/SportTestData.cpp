#include "SportTestData.h"

namespace msa::test
{
core::SportSessionTemplate sportSession()
{
    core::SportProgramItem item;
    item.identifier = "item";
    item.exercise.identifier = "exercise";
    item.exercise.revision = "exercise-v1";
    item.exercise.title = "Условное упражнение теста";
    item.exercise.published = true;
    item.exercise.technique = {"Только тест", "Только тест", {"Условный шаг"}, "Только тест", "Только тест"};
    item.exercise.muscles = {"Тестовая группа"};
    item.exercise.errors = {"Тестовая ошибка"};
    item.exercise.limitations = {"Тестовое условие"};
    item.exercise.requirements = {false, false, false, false, {}};
    item.exercise.sources = {{"Условный источник теста", "https://example.org/test-only", "2026-11-24"}};
    item.conditions.comparable = true;
    item.sets = {{"set", 8, core::BodyweightLoad{}, core::SportSide::Both}};

    return {"session", "Условное занятие теста", {item}};
}
core::SportWorkoutDraft sportDraft()
{
    const auto result = core::SportWorkoutService{}.start(std::nullopt, sportSession(), "workout",
                                                          std::chrono::year{2026} / 11 / 24, core::SportInstant{});

    return std::get<core::SportWorkoutDraft>(result);
}
core::SportWorkoutRevision sportResult()
{
    const auto draft = sportDraft();
    auto actual = draft.items.front().actual.front();
    actual.state = core::SportSetState::Performed;
    actual.repetitions = 8;
    const auto performed =
        std::get<core::SportWorkoutDraft>(core::SportWorkoutService{}.recordSet(draft, "item", actual));

    return std::get<core::SportWorkoutRevision>(core::SportWorkoutService{}.finish(performed, "revision"));
}
} // namespace msa::test
