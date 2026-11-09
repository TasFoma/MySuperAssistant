#pragma once

#include "msa/core/EventWarningRepository.h"

#include <chrono>
#include <cstdint>
#include <set>
#include <tuple>

namespace msa::core {

/// @brief Тестовое хранилище отметок предупреждений без IO.
class InMemoryEventWarningRepository final : public EventWarningRepository {
public:
    /// @brief Добавляет отметку; false, если такая тройка уже сохранена.
    [[nodiscard]] bool markIfNew(std::int64_t identifier,
        std::chrono::year_month_day occurrenceDate, EventWarningKind kind) override;

private:
    std::set<std::tuple<std::int64_t, std::chrono::year_month_day, EventWarningKind>> m_marks{};
};

}  // namespace msa::core

