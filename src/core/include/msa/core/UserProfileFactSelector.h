#pragma once

#include "msa/core/UserProfile.h"

#include <string>
#include <vector>

namespace msa::core {

/// @brief Локальный отбор памяти без хранилища, сети и изменения исходных данных.
class UserProfileFactSelector {
public:
    /// @brief Возвращает полные тексты для LlmContextInput::selectedFacts.
    /// Сначала alwaysInclude в порядке добавления, затем тематические по числу
    /// разных совпавших слов вопроса в тексте или тегах; при равенстве — порядок добавления.
    /// Слова — Unicode-буквы/числа с комбинируемыми знаками; регистр латиницы
    /// и кириллицы не учитывается. Без совпадений обычный факт исключается.
    /// При пустом вопросе возвращаются только alwaysInclude. Бюджет применяет
    /// LlmContextBuilder; приоритет не гарантирует включения при нехватке места.
    [[nodiscard]] std::vector<std::string> select(
        const UserProfile& profile, const std::string& question) const;
    /// @brief Тот же отбор с ID и метаданными; заменённые версии всегда исключаются.
    [[nodiscard]] std::vector<UserProfileFact> selectFacts(
        const UserProfile& profile, const std::string& question) const;
};

}  // namespace msa::core

