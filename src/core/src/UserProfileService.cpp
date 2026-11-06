#include "msa/core/UserProfileService.h"

#include "Utf8Text.h"

namespace msa::core {

UserProfileService::UserProfileService(UserProfileRepository& repository)
    : m_repository{repository}
{
}

UserProfileWriteResult UserProfileService::rememberFact(const std::string& fact,
    const std::vector<std::string>& tags, bool alwaysInclude)
{
    const auto trimmedFact = text::trim(fact);
    if (trimmedFact.empty()) {
        return UserProfileRejection{"Введите непустой факт о себе."};
    }
    UserProfileFact entry{.text = trimmedFact, .alwaysInclude = alwaysInclude};
    for (const auto& tag : tags) {
        const auto trimmedTag = text::trim(tag);
        if (trimmedTag.empty()) {
            return UserProfileRejection{"Введите непустой тег или уберите его."};
        }
        entry.tags.push_back(trimmedTag);
    }
    auto savedProfile = m_repository.load();
    savedProfile.facts.push_back(entry);
    m_repository.save(savedProfile);

    return std::monostate{};
}

UserProfile UserProfileService::profile() const
{
    return m_repository.load();
}

}  // namespace msa::core

