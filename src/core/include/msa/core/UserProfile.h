#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace msa::core {

/// @brief Ссылка на исходное сообщение пользователя и необязательную запись дневника.
struct ProfileFactSource {
    /// @brief ID чата исходного сообщения.
    std::int64_t chatIdentifier{0};
    /// @brief ID полного User в истории чата.
    std::int64_t messageIdentifier{0};
    /// @brief ID записи этого сообщения через MoodService, если она создана.
    std::optional<std::int64_t> moodIdentifier{};

    /// @brief Сравнивает ссылки на источник.
    [[nodiscard]] bool operator==(const ProfileFactSource&) const = default;
};

/// @brief Полный факт памяти и заданные пользователем параметры поиска.
struct UserProfileFact {
    /// @brief Исходный текст без маркеров оформления.
    std::string text{};
    /// @brief Необязательные слова или фразы для тематического поиска.
    std::vector<std::string> tags{};
    /// @brief Включать в кандидаты первым независимо от темы, в пределах бюджета.
    bool alwaysInclude{false};
    /// @brief Постоянный положительный ID, назначенный UserProfileService.
    std::int64_t identifier{0};
    /// @brief Источник автоматического извлечения; у ручного факта может отсутствовать.
    std::optional<ProfileFactSource> source{};
    /// @brief Предыдущая версия, которую уточняет эта запись.
    std::optional<std::int64_t> replacesFactIdentifier{};
    /// @brief Новая версия; при наличии старая запись исключается из контекста LLM.
    std::optional<std::int64_t> supersededByFactIdentifier{};

    /// @brief Сравнивает текст и метаданные факта.
    [[nodiscard]] bool operator==(const UserProfileFact&) const = default;
};

/// @brief Память ассистента о пользователе.
struct UserProfile {
    /// @brief Факты в UTF-8 в порядке добавления, без маркеров оформления.
    std::vector<UserProfileFact> facts{};
};

}  // namespace msa::core

