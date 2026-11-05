#pragma once

#include "msa/core/GreetingRepository.h"

#include <chrono>
#include <optional>

namespace msa::core {

/// @brief Тестовое хранилище даты приветствия без IO.
class InMemoryGreetingRepository final : public GreetingRepository {
public:
    /// @brief Возвращает последнюю сохранённую дату.
    [[nodiscard]] std::optional<std::chrono::year_month_day> lastDate() const override;
    /// @brief Заменяет дату.
    void saveDate(std::chrono::year_month_day date) override;

private:
    std::optional<std::chrono::year_month_day> m_date{};
};

}  // namespace msa::core
