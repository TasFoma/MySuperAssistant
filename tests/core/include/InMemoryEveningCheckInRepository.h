#pragma once

#include "msa/core/EveningCheckInRepository.h"

#include <chrono>
#include <optional>

namespace msa::core {

/// @brief Тестовое хранилище даты вечернего опроса без IO.
class InMemoryEveningCheckInRepository final : public EveningCheckInRepository {
public:
    /// @brief Возвращает последнюю сохранённую дату.
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override;
    /// @brief Заменяет дату.
    void saveDate(std::chrono::year_month_day date) override;
    /// @brief Возвращает копию ожидания ответа.
    [[nodiscard]] std::optional<PendingEveningCheckIn> pendingCheckIn() const override;
    /// @brief Заменяет или снимает ожидание без изменения даты выдачи.
    void savePendingCheckIn(std::optional<PendingEveningCheckIn> pending) override;

private:
    std::optional<std::chrono::year_month_day> m_date{};
    std::optional<PendingEveningCheckIn> m_pending{};
};

}  // namespace msa::core

