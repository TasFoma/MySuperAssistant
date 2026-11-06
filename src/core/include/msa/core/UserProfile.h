#pragma once

#include <string>
#include <vector>

namespace msa::core {

/// @brief Полный факт памяти и заданные пользователем параметры поиска.
struct UserProfileFact {
    /// @brief Исходный текст без маркеров оформления.
    std::string text{};
    /// @brief Необязательные слова или фразы для тематического поиска.
    std::vector<std::string> tags{};
    /// @brief Включать в кандидаты первым независимо от темы, в пределах бюджета.
    bool alwaysInclude{false};

    /// @brief Сравнивает текст и метаданные факта.
    [[nodiscard]] bool operator==(const UserProfileFact&) const = default;
};

/// @brief Память ассистента о пользователе.
struct UserProfile {
    /// @brief Факты в UTF-8 в порядке добавления, без маркеров оформления.
    std::vector<UserProfileFact> facts{};
};

}  // namespace msa::core

