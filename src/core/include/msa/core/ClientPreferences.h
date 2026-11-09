#pragma once

#include <cstdint>
#include <string>

namespace msa::core
{
/// @brief Постоянные параметры клиента; отсутствие файла означает незаполненную форму.
struct ClientPreferences
{
    std::string host{};
    std::uint16_t port{};
    std::string token{};
    bool autoConnect{true};
};
/// @brief Проверяет заполненные параметры, не включая их значения в диагностику.
[[nodiscard]] bool validClientPreferences(const ClientPreferences& preferences);
} // namespace msa::core
