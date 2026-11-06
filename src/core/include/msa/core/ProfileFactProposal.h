#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace msa::core {

/// @brief Предложенный моделью факт; источник и ID новой записи назначает программа.
struct ProfileFactProposal {
    /// @brief Краткий явно сообщённый факт или важное событие, без домыслов модели.
    std::string text{};
    /// @brief ID актуального факта из переданного контекста, если пользователь его исправил.
    std::optional<std::int64_t> replacesFactIdentifier{};
};

}  // namespace msa::core

