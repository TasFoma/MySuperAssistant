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
    virtual void correctWorkout(const SportWorkoutRevision& value, const std::string& expectedRevision) = 0;
    [[nodiscard]] virtual std::vector<SportWorkoutRecord> workoutHistory() const = 0;
};
} // namespace msa::core
