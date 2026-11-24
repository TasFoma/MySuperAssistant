#include "msa/core/SportTimer.h"

#include <algorithm>
#include <limits>

namespace msa::core
{
namespace
{
constexpr auto kMaximumManualDuration = std::chrono::hours{24 * 7};
}
SportTimerState startSportTimer(SportInstant instant)
{
    SportTimerState timer;
    timer.begin = instant;
    timer.observed = instant;

    return timer;
}
SportResult<SportTimerState> observeSportTimer(const SportTimerState& timer, SportTimeObservation observation)
{
    if (observation.elapsed < SportDuration::zero() || observation.elapsed > kMaximumManualDuration)
    {
        return SportRejection{"time", "Некорректный измеренный интервал времени."};
    }
    auto result = timer;
    result.clockWarning = result.clockWarning || observation.instant < timer.observed;
    // Последовательные наблюдения одной фазы не раздувают checkpoint при каждом символе ввода.
    if (!result.clockWarning && !result.segments.empty() && result.segments.back().end == timer.observed &&
        result.segments.back().active == !timer.paused)
    {
        auto& segment = result.segments.back();
        segment.end = observation.instant;
        segment.elapsed += observation.elapsed;
    }
    else
    {
        result.segments.push_back({timer.observed, observation.instant, observation.elapsed, !timer.paused});
    }
    result.observed = observation.instant;

    return result;
}
SportTimerState pauseSportTimer(const SportTimerState& timer)
{
    auto result = timer;
    result.paused = true;

    return result;
}
SportTimerState resumeSportTimer(const SportTimerState& timer)
{
    auto result = timer;
    result.paused = false;

    return result;
}
SportResult<SportTimerState> registerSportAbsence(const SportTimerState& timer, SportAbsenceInterval interval)
{
    if (interval.identifier.empty())
    {
        return SportRejection{"interval", "Укажите идентичность интервала."};
    }
    for (const auto& existing : timer.absences)
    {
        if (existing.identifier == interval.identifier)
        {
            if (existing.begin != interval.begin || existing.end != interval.end)
            {
                return SportRejection{"interval", "Идентичность относится к другому интервалу."};
            }
            return timer;
        }
        if (!timer.clockWarning && existing.end >= existing.begin && interval.end >= interval.begin &&
            interval.begin < existing.end && existing.begin < interval.end)
        {
            return SportRejection{"interval", "Интервалы отсутствия пересекаются."};
        }
    }
    if (interval.begin != timer.observed)
    {
        return SportRejection{"interval", "Отсутствие должно начинаться с последнего наблюдения."};
    }
    auto result = timer;
    interval.active.reset();
    interval.decision.reset();
    if (interval.end >= interval.begin && !timer.clockWarning)
    {
        interval.duration = interval.end - interval.begin;
    }
    else
    {
        interval.duration.reset();
        result.clockWarning = true;
    }
    result.observed = interval.end;
    result.absences.push_back(interval);

    return result;
}
SportResult<SportTimerState> resolveSportAbsence(const SportTimerState& timer, const std::string& identifier,
                                                 SportAbsenceDecision decision, std::optional<SportDuration> manual)
{
    auto result = timer;
    const auto iterator = std::ranges::find(result.absences, identifier, &SportAbsenceInterval::identifier);
    if (iterator == result.absences.end())
    {
        return SportRejection{"interval", "Интервал не найден."};
    }
    if (decision != SportAbsenceDecision::Activity && decision != SportAbsenceDecision::Pause &&
        decision != SportAbsenceDecision::Manual)
    {
        return SportRejection{"interval", "Выберите способ учёта времени."};
    }
    SportDuration active{};
    if (decision == SportAbsenceDecision::Activity)
    {
        if (!iterator->duration)
        {
            return SportRejection{"clock", "Часы изменились. Укажите активную длительность вручную."};
        }
        active = *iterator->duration;
    }
    else if (decision == SportAbsenceDecision::Manual)
    {
        if (!manual || *manual < SportDuration::zero() || *manual > kMaximumManualDuration ||
            (iterator->duration && *manual > *iterator->duration))
        {
            return SportRejection{"time", "Укажите допустимую активную часть интервала."};
        }
        active = *manual;
    }
    else if (manual)
    {
        return SportRejection{"time", "Для паузы ручное значение не применяется."};
    }
    iterator->active = active;
    iterator->decision = decision;

    return result;
}
SportDurationSummary summarizeSportDuration(const SportTimerState& timer)
{
    SportDurationSummary result;
    result.warning = timer.clockWarning;
    result.corrected = timer.manualDuration;
    if (!timer.clockWarning && timer.observed >= timer.begin)
    {
        result.total = timer.observed - timer.begin;
    }
    for (const auto& segment : timer.segments)
    {
        if (segment.active)
        {
            result.active += segment.elapsed;
        }
    }
    for (const auto& interval : timer.absences)
    {
        if (interval.active)
        {
            result.active += *interval.active;
        }
        else
        {
            result.unresolved = true;
        }
    }

    return result;
}
} // namespace msa::core
