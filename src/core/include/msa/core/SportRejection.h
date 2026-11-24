#pragma once

#include <string>
#include <variant>

namespace msa::core
{
/// @brief Предметный отказ спорта без изменения исходного состояния.
struct SportRejection
{
    std::string code{};
    std::string reason{};
};
/// @brief Значение успешной операции либо объяснение отказа.
template <typename ValueType> using SportResult = std::variant<ValueType, SportRejection>;
} // namespace msa::core
