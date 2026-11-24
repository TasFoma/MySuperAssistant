#include "msa/core/SportLoad.h"

#include <algorithm>
#include <type_traits>

namespace msa::core
{
namespace
{
constexpr std::int32_t kMaximumMass = 100000;
}
SportResult<std::optional<std::int32_t>> parseSportMass(std::string_view text)
{
    if (text.empty())
    {
        return std::optional<std::int32_t>{};
    }
    std::int32_t amount{};
    int fractional{-1};
    for (const auto character : text)
    {
        if ((character == '.' || character == ',') && fractional == -1)
        {
            fractional = 0;
            continue;
        }
        if (character < '0' || character > '9' || fractional == 2)
        {
            return SportRejection{"mass", "Укажите массу с точностью до двух знаков."};
        }
        if (amount > kMaximumMass)
        {
            return SportRejection{"mass", "Масса больше 1000 кг."};
        }
        amount = amount * 10 + character - '0';
        if (fractional >= 0)
        {
            ++fractional;
        }
    }
    if (text.front() == '.' || text.front() == ',' || fractional == 0)
    {
        return SportRejection{"mass", "Завершите ввод массы."};
    }
    const auto multiplier = fractional < 0 ? 100 : (fractional == 1 ? 10 : 1);
    if (amount > kMaximumMass / multiplier)
    {
        return SportRejection{"mass", "Масса больше 1000 кг."};
    }

    return std::optional<std::int32_t>{amount * multiplier};
}
std::optional<SportRejection> validateSportLoad(const SportLoad& load, bool performed)
{
    const auto validMass = [](const std::optional<std::int32_t>& mass, bool positive)
    { return !mass || (*mass >= (positive ? 1 : 0) && *mass <= kMaximumMass); };
    const auto valid = std::visit(
        [performed, validMass](const auto& value)
        {
            using LoadType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<LoadType, BodyweightLoad>)
            {
                return !(value.extra && value.assistance) && validMass(value.extra, false) &&
                       validMass(value.assistance, false);
            }
            else if constexpr (std::is_same_v<LoadType, DumbbellLoad>)
            {
                return (value.quantity == 1 || value.quantity == 2) && validMass(value.mass, true) &&
                       (!performed || value.mass.has_value());
            }
            else
            {
                return validMass(value.mass, true) && (!performed || value.mass.has_value());
            }
        },
        load);
    if (!valid)
    {
        return SportRejection{"load", "Проверьте режим нагрузки, массу и количество инвентаря."};
    }

    return std::nullopt;
}
} // namespace msa::core
