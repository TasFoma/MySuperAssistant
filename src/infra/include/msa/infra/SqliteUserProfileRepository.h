#pragma once

#include "msa/core/UserProfileRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище полного профиля, источников, версий и метаданных.
/// Владелец базы со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteUserProfileRepository final : public core::UserProfileRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteUserProfileRepository(SqliteDatabase& database);
    /// @brief Возвращает все факты и теги в сохранённом порядке; изначально профиль пуст.
    [[nodiscard]] core::UserProfile load() const override;
    /// @brief Атомарно заменяет профиль целиком, сохраняя ID и порядок из снимка.
    /// Ошибка SQL откатывает весь save и передаётся как std::runtime_error.
    /// Участвует во внешней транзакции SqliteDatabase; ID назначает сервис core.
    void save(const core::UserProfile& profile) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

