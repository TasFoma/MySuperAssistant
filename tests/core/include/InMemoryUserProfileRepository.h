#pragma once

#include "msa/core/UserProfileRepository.h"

namespace msa::core {

/// @brief Тестовое хранилище единственного профиля.
class InMemoryUserProfileRepository final : public UserProfileRepository {
public:
    /// @brief Возвращает независимую копию профиля.
    [[nodiscard]] UserProfile load() const override;
    /// @brief Заменяет профиль целиком.
    void save(const UserProfile& profile) override;

private:
    UserProfile m_profile{};
};

}  // namespace msa::core

