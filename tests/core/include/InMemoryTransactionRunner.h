#pragma once

#include "InMemoryChatRepository.h"
#include "InMemoryEveningCheckInRepository.h"
#include "InMemoryMoodRepository.h"
#include "InMemoryUserProfileRepository.h"

#include "msa/core/TransactionRunner.h"

namespace msa::core {

/// @brief Тестовая атомарная операция над четырьмя хранилищами связки чата.
class InMemoryTransactionRunner final : public TransactionRunner {
public:
    /// @brief Принимает те же хранилища, которые используют сервисы чата.
    InMemoryTransactionRunner(InMemoryChatRepository& chats, InMemoryMoodRepository& moods,
        InMemoryUserProfileRepository& profiles, InMemoryEveningCheckInRepository& checkIns);
    /// @brief При исключении восстанавливает снимки, включая счётчики ID.
    void transaction(const std::function<void()>& operation) override;
    /// @brief Позволяет проверить отсутствие транзакции во время вызова модели.
    [[nodiscard]] bool isActive() const;
    /// @brief Имитирует ошибку следующей фиксации после выполнения callback.
    void failNextCommit();

private:
    InMemoryChatRepository& m_chats;
    InMemoryMoodRepository& m_moods;
    InMemoryUserProfileRepository& m_profiles;
    InMemoryEveningCheckInRepository& m_checkIns;
    bool m_active{false};
    bool m_failCommit{false};
};

}  // namespace msa::core

