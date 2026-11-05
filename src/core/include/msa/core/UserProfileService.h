#pragma once

#include "msa/core/UserProfileRepository.h"

#include <string>
#include <variant>

namespace msa::core {

/// @brief Причина отказа добавления факта.
struct UserProfileRejection {
    std::string reason;
};

/// @brief Успешное добавление либо причина отказа.
using UserProfileWriteResult = std::variant<std::monostate, UserProfileRejection>;

/// @brief Память пользователя без автоматического удаления фактов.
/// Технические исключения хранилища пробрасываются вызывающему коду.
class UserProfileService {
public:
    /// @brief Репозиторий не принадлежит сервису и должен жить дольше него.
    explicit UserProfileService(UserProfileRepository& repository);
    /// @brief Обрезает пробельные символы Unicode по краям и добавляет непустой факт.
    /// Повторы сохраняются; внутренние переносы строк остаются частью одного факта.
    [[nodiscard]] UserProfileWriteResult rememberFact(const std::string& fact);
    /// @brief Возвращает независимую копию всех фактов в порядке добавления.
    [[nodiscard]] UserProfile profile() const;

private:
    UserProfileRepository& m_repository;
};

}  // namespace msa::core

