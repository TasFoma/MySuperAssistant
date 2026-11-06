#pragma once

#include "msa/core/ChatRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище полных чатов, сообщений и текущего выбора.
/// База должна быть открыта с sqliteMigrations() и жить дольше репозитория.
/// Работа только в потоке владельца базы; ошибки SQL — std::runtime_error.
class SqliteChatRepository final : public core::ChatRepository {
public:
    /// @brief Сохраняет невладеющую ссылку на базу с предметной схемой.
    explicit SqliteChatRepository(SqliteDatabase& database);

    /// @brief Сохраняет новый чат с нулевым ID, назначает положительный ID.
    [[nodiscard]] std::int64_t addChat(const core::Chat& chat) override;
    /// @brief Возвращает копию чата либо nullopt.
    [[nodiscard]] std::optional<core::Chat> findChat(std::int64_t identifier) const override;
    /// @brief Возвращает независимые копии всех чатов.
    [[nodiscard]] std::vector<core::Chat> chats() const override;
    /// @brief Сохраняет сообщение с нулевым ID; nullopt при отсутствии чата.
    [[nodiscard]] std::optional<std::int64_t> addMessage(const core::ChatMessage& message) override;
    /// @brief Возвращает полные сообщения указанного чата; пустой список при отсутствии.
    [[nodiscard]] std::vector<core::ChatMessage> messages(std::int64_t chatIdentifier) const override;
    /// @brief Возвращает сохранённый выбор либо nullopt до первого сохранения.
    [[nodiscard]] std::optional<std::int64_t> currentChatId() const override;
    /// @brief Сохраняет существующий чат; false без изменения при отсутствии.
    bool saveCurrentChatId(std::int64_t identifier) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

