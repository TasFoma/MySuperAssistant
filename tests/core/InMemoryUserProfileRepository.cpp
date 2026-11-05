#include "InMemoryUserProfileRepository.h"

namespace msa::core {

UserProfile InMemoryUserProfileRepository::load() const
{
    return m_profile;
}

void InMemoryUserProfileRepository::save(const UserProfile& profile)
{
    m_profile = profile;
}

}  // namespace msa::core

