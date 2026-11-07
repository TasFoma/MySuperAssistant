#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace msa::core
{

/// @brief Причина отказа внешнего сервиса без технических секретов.
enum class ExternalServiceErrorKind : std::uint8_t
{
    InvalidRequest,
    NotFound,
    Unavailable,
    InvalidResponse,
};

/// @brief Ожидаемый отказ без частичного результата.
struct ExternalServiceError
{
    /// @brief Категория для вызывающего кода.
    ExternalServiceErrorKind kind{ExternalServiceErrorKind::Unavailable};
    /// @brief Понятная пользователю причина.
    std::string reason{};
};

/// @brief Полный результат либо ожидаемая ошибка сервиса.
template <typename ValueType> using ExternalServiceResult = std::variant<ValueType, ExternalServiceError>;

} // namespace msa::core
