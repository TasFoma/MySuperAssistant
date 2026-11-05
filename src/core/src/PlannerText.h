#pragma once

#include <chrono>
#include <string>

namespace msa::core::text {

/// @brief Русская дата с днём недели; ожидается корректная дата.
[[nodiscard]] std::string russianDate(std::chrono::year_month_day date);
/// @brief Время HH:MM; ожидаются минуты от начала суток в пределах одного дня.
[[nodiscard]] std::string timeText(std::chrono::minutes time);

}  // namespace msa::core::text

