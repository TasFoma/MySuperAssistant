#pragma once

#include "msa/core/ProfileFactProposal.h"
#include "msa/core/UserProfileRepository.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Причина отказа добавления факта.
struct UserProfileRejection {
    std::string reason;
};

/// @brief Успешное добавление либо причина отказа.
using UserProfileWriteResult = std::variant<std::monostate, UserProfileRejection>;
/// @brief Новые сохранённые версии либо отказ; точные повторы не возвращаются.
using ProfileFactImportResult = std::variant<std::vector<UserProfileFact>, UserProfileRejection>;

/// @brief Память пользователя без автоматического удаления фактов.
/// Технические исключения хранилища пробрасываются вызывающему коду.
class UserProfileService {
public:
    /// @brief Репозиторий не принадлежит сервису и должен жить дольше него.
    explicit UserProfileService(UserProfileRepository& repository);
    /// @brief Обрезает пробельные символы Unicode по краям и добавляет непустой факт.
    /// Повторы сохраняются; внутренние переносы строк остаются частью одного факта.
    /// Теги обрезаются по краям; пустой тег отклоняет всю операцию.
    /// alwaysInclude даёт приоритет при отборе, но не отменяет бюджет контекста.
    [[nodiscard]] UserProfileWriteResult rememberFact(const std::string& fact,
        const std::vector<std::string>& tags = {}, bool alwaysInclude = false);
    /// @brief Возвращает независимую копию всех фактов в порядке добавления.
    [[nodiscard]] UserProfile profile() const;
    /// @brief Проверяет пакет на снимке без записи, перед другими действиями связки.
    /// Пустой текст, неактуальная/неизвестная цель или неоднозначная замена — отказ.
    [[nodiscard]] std::optional<UserProfileRejection> validateExtractedFacts(
        const std::vector<ProfileFactProposal>& proposals) const;
    /// @brief Добавляет пакет с источником одним сохранением профиля; старые версии остаются.
    /// Точные повторы после trim пропускаются без учёта возраста, похожие не склеиваются.
    /// Замена наследует теги и приоритет; повтор старого текста как исправление допустим.
    /// Неверный пакет не меняет хранилище. Проверка и запись не образуют транзакцию
    /// с другими репозиториями; конкурентные изменения профиля не поддерживаются.
    [[nodiscard]] ProfileFactImportResult rememberExtractedFacts(
        const std::vector<ProfileFactProposal>& proposals, const ProfileFactSource& source);

private:
    UserProfileRepository& m_repository;
};

}  // namespace msa::core

