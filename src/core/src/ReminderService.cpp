#include "msa/core/ReminderService.h"

#include <stdexcept>

namespace msa::core {

namespace {

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
