#include "msa/core/MoodService.h"

#include <algorithm>
#include <chrono>
#include <tuple>

namespace msa::core {

MoodService::MoodService(const Clock& clock, MoodRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
}

MoodWriteResult MoodService::logMood(MoodScore score, const std::string& note)
{
    if (score != MoodScore::Negative && score != MoodScore::Neutral && score != MoodScore::Positive) {
        return MoodRejection{"Выберите отрицательное, нейтральное или положительное настроение."};
    }

    return m_repository.add(MoodEntry{.recordedAt = m_clock.now(), .score = score, .note = note});
}

MoodHistoryResult MoodService::moodHistory(int days) const
{
    if (days <= 0) {
        return MoodRejection{"Число дней истории должно быть положительным."};
    }
    const auto now = m_clock.now();
    const auto firstDay = std::chrono::floor<std::chrono::days>(now)
        - std::chrono::days{days - 1};
    auto entries = m_repository.all();
    std::erase_if(entries, [firstDay, now](const MoodEntry& entry) {
        return std::chrono::floor<std::chrono::days>(entry.recordedAt) < firstDay
            || entry.recordedAt > now;
    });
    std::ranges::sort(entries, [](const MoodEntry& left, const MoodEntry& right) {
        return std::tie(left.recordedAt, left.identifier) < std::tie(right.recordedAt, right.identifier);
    });

    return entries;
}

}  // namespace msa::core

