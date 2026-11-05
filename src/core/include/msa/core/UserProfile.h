#pragma once

#include <string>
#include <vector>

namespace msa::core {

/// @brief Память ассистента о пользователе.
struct UserProfile {
    /// @brief Факты в UTF-8 в порядке добавления, без маркеров оформления.
    std::vector<std::string> facts{};
};

}  // namespace msa::core

