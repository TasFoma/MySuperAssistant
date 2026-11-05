#pragma once

#include "msa/core/UserProfile.h"

namespace msa::core {

/// @brief Хранилище единственного профиля пользователя.
/// Технические ошибки передаются исключениями без Qt-типов.
class UserProfileRepository {
public:
    /// @brief Освобождает реализацию хранилища.
    virtual ~UserProfileRepository();

    /// @brief Возвращает копию профиля; до первого сохранения список фактов пуст.
    [[nodiscard]] virtual UserProfile load() const = 0;
    /// @brief Заменяет профиль целиком; ограничения количества фактов здесь не применяются.
    virtual void save(const UserProfile& profile) = 0;
};

}  // namespace msa::core

