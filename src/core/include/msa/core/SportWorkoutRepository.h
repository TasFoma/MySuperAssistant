#pragma once

#include "msa/core/SportWorkout.h"

#include <cstdint>
#include <map>

namespace msa::core
{
/// @brief Проверенное состояние и ещё не разобранный редакторский ввод.
struct SportCheckpoint
{
    SportWorkoutDraft draft{};
    std::map<std::string, std::string> editorText{};
    std::uint64_t sequence{0};
};
/// @brief Отдельный устойчивый черновик исправления; исходная редакция не подменяется новой головой.
struct SportCorrectionCheckpoint
{
    std::string identifier{};
    SportWorkoutRevision original{};
    SportCheckpoint edited{};
};
/// @brief Постоянные локальные тренировки; ошибки записи передаются исключениями.
class SportWorkoutRepository
{
public:
    virtual ~SportWorkoutRepository();
    [[nodiscard]] virtual std::optional<SportCheckpoint> activeWorkout() const = 0;
    /// @brief Записывает checkpoint с проверкой ожидаемого номера; возвращает новый номер.
    [[nodiscard]] virtual std::uint64_t checkpoint(const SportCheckpoint& value, std::uint64_t expectedSequence) = 0;
    /// @brief Атомарно завершает активное занятие; повтор той же редакции безопасен.
    virtual void finishWorkout(const SportWorkoutRevision& value, std::uint64_t expectedSequence) = 0;
    /// @brief Исправляет результат только поверх ожидаемой текущей редакции.
    virtual void correctWorkout(const SportWorkoutRevision& value, const std::string& expectedRevision,
                                std::optional<std::uint64_t> expectedDraftSequence = std::nullopt,
                                const std::optional<std::string>& expectedEditingIdentifier = std::nullopt) = 0;
    /// @brief Сохраняет неполное исправление отдельно от активного занятия и результата.
    [[nodiscard]] virtual std::uint64_t checkpointCorrection(const SportCorrectionCheckpoint& value,
                                                             std::uint64_t expectedSequence) = 0;
    [[nodiscard]] virtual std::vector<SportCorrectionCheckpoint> correctionDrafts() const = 0;
    /// @brief Удаляет только явно отменённый черновик с проверкой идентичности и номера.
    virtual void discardCorrection(const std::string& workoutIdentifier, const std::string& editingIdentifier,
                                   std::uint64_t expectedSequence) = 0;
    [[nodiscard]] virtual std::vector<SportWorkoutRecord> workoutHistory() const = 0;
};
} // namespace msa::core
