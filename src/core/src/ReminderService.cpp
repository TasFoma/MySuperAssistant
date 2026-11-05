#include "msa/core/ReminderService.h"

#include "Utf8Text.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace msa::core {

namespace {

constexpr double kEarthRadiusMeters = 6371000.0;
constexpr double kPlaceRadiusMeters = 200.0;
constexpr double kMaxLatitude = 90.0;
constexpr double kMaxLongitude = 180.0;
constexpr double kRadiansPerDegree = std::numbers::pi / kMaxLongitude;

[[nodiscard]] bool validCoordinates(GeoCoordinates coordinates)
{
    return std::isfinite(coordinates.latitude) && std::isfinite(coordinates.longitude)
           && std::abs(coordinates.latitude) <= kMaxLatitude
           && std::abs(coordinates.longitude) <= kMaxLongitude;
}

[[nodiscard]] double distanceMeters(GeoCoordinates first, GeoCoordinates second)
{
    const auto halfLatitude = (second.latitude - first.latitude) * kRadiansPerDegree / 2.0;
    const auto halfLongitude = (second.longitude - first.longitude) * kRadiansPerDegree / 2.0;
    const auto latitudeSine = std::sin(halfLatitude);
    const auto longitudeSine = std::sin(halfLongitude);
    const auto haversine = latitudeSine * latitudeSine
        + std::cos(first.latitude * kRadiansPerDegree)
          * std::cos(second.latitude * kRadiansPerDegree) * longitudeSine * longitudeSine;
    // Ограничение компенсирует округление у совпадающих и противоположных точек.
    return 2.0 * kEarthRadiusMeters * std::asin(std::sqrt(std::clamp(haversine, 0.0, 1.0)));
}

[[nodiscard]] bool canSchedule(std::chrono::local_time<std::chrono::seconds> now,
                               std::chrono::minutes interval)
{
    if (interval <= std::chrono::minutes::zero()
        || interval > std::chrono::duration_cast<std::chrono::minutes>(std::chrono::seconds::max())) {
        return false;
    }
    const auto delay = std::chrono::duration_cast<std::chrono::seconds>(interval);

    return now.time_since_epoch() <= std::chrono::seconds::max() - delay;
}

void saveUpdate(ReminderRepository& repository, const Reminder& reminder)
{
    if (!repository.update(reminder)) {
        throw std::runtime_error{"Напоминание исчезло во время обработки"};
    }
}

}  // namespace

ReminderService::ReminderService(const Clock& clock, ReminderRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
}

ReminderCreationResult ReminderService::addTimed(
    const std::string& title, std::chrono::local_time<std::chrono::seconds> dueAt)
{
    if (dueAt < m_clock.now()) {
        return ReminderRejection{"Время напоминания уже прошло. Укажите другое время."};
    }

    return m_repository.add(TimedReminder{.title = title, .dueAt = dueAt});
}

ReminderCreationResult ReminderService::addRecurring(
    const std::string& title, std::chrono::minutes interval)
{
    if (interval <= std::chrono::minutes::zero()) {
        return ReminderRejection{"Интервал должен быть больше нуля. Укажите другой интервал."};
    }
    const auto now = m_clock.now();
    if (!canSchedule(now, interval)) {
        return ReminderRejection{"Интервал слишком большой. Укажите другой интервал."};
    }

    return m_repository.add(RecurringReminder{
        .title = title, .dueAt = now + interval, .interval = interval});
}

ReminderCreationResult ReminderService::addPlace(const std::string& title,
    const std::string& place, std::optional<GeoCoordinates> coordinates)
{
    if (text::charCount(title) < 1 || text::charCount(place) < 1) {
        return ReminderRejection{"Введите название напоминания и место."};
    }
    if (coordinates && !validCoordinates(*coordinates)) {
        return ReminderRejection{"Укажите корректные координаты места."};
    }

    return m_repository.add(PlaceReminder{.title = title, .place = place, .coordinates = coordinates});
}

PlaceCheckResult ReminderService::checkNearPlace(GeoCoordinates position)
{
    if (!validCoordinates(position)) {
        return ReminderRejection{"Укажите корректную текущую позицию."};
    }
    std::vector<PlaceReminder> due;
    for (auto reminder : m_repository.all()) {
        auto* place = std::get_if<PlaceReminder>(&reminder);
        if (place == nullptr || place->status != ReminderStatus::Pending || !place->coordinates) {
            continue;
        }
        if (!validCoordinates(*place->coordinates)) {
            throw std::runtime_error{"Некорректные координаты сохранённого напоминания"};
        }
        if (distanceMeters(position, *place->coordinates) >= kPlaceRadiusMeters) {
            continue;
        }
        const auto original = *place;
        place->status = ReminderStatus::Fired;
        saveUpdate(m_repository, reminder);
        due.push_back(original);
    }

    return due;
}

void ReminderService::skipMissedOnStartup()
{
    const auto now = m_clock.now();
    for (auto reminder : m_repository.all()) {
        auto* timed = std::get_if<TimedReminder>(&reminder);
        if (timed == nullptr || timed->status != ReminderStatus::Pending || timed->dueAt >= now) {
            continue;
        }
        timed->status = ReminderStatus::Missed;
        saveUpdate(m_repository, reminder);
    }
}

std::vector<Reminder> ReminderService::checkDue()
{
    const auto now = m_clock.now();
    std::vector<Reminder> due;
    for (auto reminder : m_repository.all()) {
        const Reminder original = reminder;
        if (auto* timed = std::get_if<TimedReminder>(&reminder)) {
            if (timed->status != ReminderStatus::Pending || timed->dueAt > now) {
                continue;
            }
            timed->status = ReminderStatus::Fired;
        } else if (auto* recurring = std::get_if<RecurringReminder>(&reminder)) {
            if (recurring->dueAt > now) {
                continue;
            }
            if (!canSchedule(now, recurring->interval)) {
                throw std::runtime_error{"Некорректный интервал сохранённого напоминания"};
            }
            recurring->dueAt = now + recurring->interval;
        } else {
            continue;
        }
        saveUpdate(m_repository, reminder);
        due.push_back(original);
    }

    return due;
}

}  // namespace msa::core
