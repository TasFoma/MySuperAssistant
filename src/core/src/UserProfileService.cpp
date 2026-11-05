#include "msa/core/UserProfileService.h"

#include "Utf8Text.h"

namespace msa::core {

UserProfileService::UserProfileService(UserProfileRepository& repository)
    : m_repository{repository}
{
}

UserProfileWriteResult UserProfileService::rememberFact(const std::string& fact)
{
    const auto trimmedFact = text::trim(fact);
    if (trimmedFact.empty()) {
        return UserProfileRejection{"Введите непустой факт о себе."};
    }
    auto savedProfile = m_repository.load();
    savedProfile.facts.push_back(trimmedFact);
    m_repository.save(savedProfile);

    return std::monostate{};
}

UserProfile UserProfileService::profile() const
{
    return m_repository.load();
}

}  // namespace msa::core

