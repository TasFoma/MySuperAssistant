#pragma once

#include "msa/core/SportRejection.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace msa::core
{
/// @brief Масса одной гантели в сотых килограмма и количество гантелей.
struct DumbbellLoad
{
    std::optional<std::int32_t> mass{};
    int quantity{2};
    bool operator==(const DumbbellLoad&) const = default;
};
/// @brief Полная масса штанги с грифом в сотых килограмма.
struct BarbellLoad
{
    std::optional<std::int32_t> mass{};
    bool operator==(const BarbellLoad&) const = default;
};
/// @brief Собственный вес с необязательным отягощением или противовесом.
struct BodyweightLoad
{
    std::optional<std::int32_t> extra{};
    std::optional<std::int32_t> assistance{};
    bool operator==(const BodyweightLoad&) const = default;
};
using SportLoad = std::variant<BodyweightLoad, DumbbellLoad, BarbellLoad>;
/// @brief Разбирает массу точно, без округления; пустая строка означает отсутствие.
[[nodiscard]] SportResult<std::optional<std::int32_t>> parseSportMass(std::string_view text);
/// @brief Проверяет применимые поля и необходимость указанной массы при выполнении.
[[nodiscard]] std::optional<SportRejection> validateSportLoad(const SportLoad& load, bool performed = false);
} // namespace msa::core
