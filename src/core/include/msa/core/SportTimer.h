#pragma once

#include "msa/core/SportRejection.h"

#include <chrono>
#include <optional>
#include <vector>

namespace msa::core
{
using SportInstant = std::chrono::sys_time<std::chrono::milliseconds>;
using SportDuration = std::chrono::milliseconds;
struct SportTimeObservation
{
    SportInstant instant{};
    SportDuration elapsed{};
};
struct SportTimerSegment
{
    SportInstant begin{};
    SportInstant end{};
    SportDuration elapsed{};
    bool active{false};
};
enum class SportAbsenceDecision
{
    Activity,
    Pause,
    Manual
};
struct SportAbsenceInterval
{
    std::string identifier{};
    SportInstant begin{};
    SportInstant end{};
    std::optional<SportDuration> duration{};
    std::optional<SportDuration> active{};
    std::optional<SportAbsenceDecision> decision{};
};
/// @brief Состояние таймера переносится значением; часы и файлы core не читает.
struct SportTimerState
{
    SportInstant begin{};
    SportInstant observed{};
    bool paused{false};
    bool clockWarning{false};
    std::vector<SportTimerSegment> segments{};
    std::vector<SportAbsenceInterval> absences{};
    std::optional<SportDuration> manualDuration{};
};
struct SportDurationSummary
{
    std::optional<SportDuration> total{};
    SportDuration active{};
    std::optional<SportDuration> corrected{};
    bool unresolved{false};
    bool warning{false};
};
/// @brief Начинает таймер с переданной исходной отметки.
[[nodiscard]] SportTimerState startSportTimer(SportInstant instant);
/// @brief Добавляет измеренный монотонными часами интервал; отрицательный отклоняется.
[[nodiscard]] SportResult<SportTimerState> observeSportTimer(const SportTimerState& timer,
                                                             SportTimeObservation observation);
/// @brief Пауза и продолжение не добавляют длительность сами по себе.
[[nodiscard]] SportTimerState pauseSportTimer(const SportTimerState& timer);
[[nodiscard]] SportTimerState resumeSportTimer(const SportTimerState& timer);
/// @brief Регистрирует отсутствие без автоматического включения в активность.
[[nodiscard]] SportResult<SportTimerState> registerSportAbsence(const SportTimerState& timer,
                                                                SportAbsenceInterval interval);
/// @brief Заменяет решение того же интервала; повтор не удваивает вклад.
[[nodiscard]] SportResult<SportTimerState> resolveSportAbsence(const SportTimerState& timer,
                                                               const std::string& identifier,
                                                               SportAbsenceDecision decision,
                                                               std::optional<SportDuration> manual = std::nullopt);
/// @brief Раздельно возвращает общее, активное и скорректированное время.
[[nodiscard]] SportDurationSummary summarizeSportDuration(const SportTimerState& timer);
} // namespace msa::core
