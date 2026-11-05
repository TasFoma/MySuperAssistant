#pragma once

#include "msa/core/Clock.h"
#include "msa/core/MoodRepository.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Причина отказа из-за неверных параметров дневника.
struct MoodRejection {
    /// @brief Объяснение для вызывающей стороны.
    std::string reason;
};

/// @brief ID сохранённой записи либо причина отказа.
using MoodWriteResult = std::variant<std::int64_t, MoodRejection>;
/// @brief История настроения либо причина отказа для неверного периода.
using MoodHistoryResult = std::variant<std::vector<MoodEntry>, MoodRejection>;

/// @brief Явная запись настроения и чтение истории без анализа текста и очистки данных.
class MoodService {
public:
    /// @brief Число календарных дней истории при вызове без периода.
    static constexpr int kDefaultHistoryDays{14};

    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    MoodService(const Clock& clock, MoodRepository& repository);
    /// @brief Сохраняет оценку Negative/Neutral/Positive с текущим временем из Clock.
    /// Заметка сохраняется полностью; другие значения оценки отклоняются без записи.
    [[nodiscard]] MoodWriteResult logMood(MoodScore score, const std::string& note = "");
    /// @brief Возвращает копии за последние days календарных дней, включая сегодня.
    /// Нижняя граница — полночь первого дня, верхняя — сейчас, обе включены.
    /// Порядок — по времени и ID. days <= 0 отклоняется; старые записи не удаляются.
    /// Технические ошибки репозитория пробрасываются исключениями.
    [[nodiscard]] MoodHistoryResult moodHistory(int days = kDefaultHistoryDays) const;

private:
    const Clock& m_clock;
    MoodRepository& m_repository;
};

}  // namespace msa::core

