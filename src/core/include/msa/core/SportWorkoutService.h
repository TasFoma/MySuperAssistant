#pragma once

#include "msa/core/SportWorkout.h"

namespace msa::core
{
/// @brief Чистые операции над переданными снимками, без хранилища и системных часов.
class SportWorkoutService final
{
public:
    /// @brief Начинает занятие; второе активное состояние отклоняется.
    [[nodiscard]] SportResult<SportWorkoutDraft> start(const std::optional<SportWorkoutDraft>& active,
                                                       const SportSessionTemplate& session,
                                                       const std::string& identifier, std::chrono::year_month_day date,
                                                       SportInstant instant) const;
    /// @brief Отмечает подход выполненным с явными фактическими значениями.
    [[nodiscard]] SportResult<SportWorkoutDraft>
    recordSet(const SportWorkoutDraft& draft, const std::string& itemIdentifier, const SportActualSet& actual) const;
    /// @brief Добавляет внеплановый подход; максимум 100 на пункт.
    [[nodiscard]] SportResult<SportWorkoutDraft>
    addSet(const SportWorkoutDraft& draft, const std::string& itemIdentifier, const SportActualSet& actual) const;
    /// @brief Пропуск не становится выполнением с нулевыми повторениями.
    [[nodiscard]] SportResult<SportWorkoutDraft>
    skipSet(const SportWorkoutDraft& draft, const std::string& itemIdentifier, const std::string& setIdentifier) const;
    [[nodiscard]] SportResult<SportWorkoutDraft> skipExercise(const SportWorkoutDraft& draft,
                                                              const std::string& itemIdentifier) const;
    /// @brief Добавляет явно выбранный пункт вне исходного плана, сохраняя исходный снимок.
    [[nodiscard]] SportResult<SportWorkoutDraft> addExercise(const SportWorkoutDraft& draft,
                                                             const SportProgramItem& item) const;
    /// @brief Выполненное остаётся у исходного упражнения; новый план явно задан.
    [[nodiscard]] SportResult<SportWorkoutDraft> replaceExercise(const SportWorkoutDraft& draft,
                                                                 const std::string& itemIdentifier,
                                                                 const SportProgramItem& replacement) const;
    /// @brief Проверяет итог и готовит первую редакцию без записи.
    [[nodiscard]] SportResult<SportWorkoutRevision> finish(const SportWorkoutDraft& draft,
                                                           const std::string& revisionIdentifier) const;
    /// @brief Готовит исправление той же тренировки с проверкой ожидаемой редакции.
    [[nodiscard]] SportResult<SportWorkoutRevision> correct(const SportWorkoutRevision& current,
                                                            const std::string& expectedRevision,
                                                            const SportWorkoutDraft& edited,
                                                            const std::string& revisionIdentifier) const;
};
} // namespace msa::core
