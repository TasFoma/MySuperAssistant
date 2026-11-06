#include "InMemoryTransactionRunner.h"

#include <stdexcept>
#include <utility>

namespace msa::core {

InMemoryTransactionRunner::InMemoryTransactionRunner(InMemoryChatRepository& chats,
    InMemoryMoodRepository& moods, InMemoryUserProfileRepository& profiles,
    InMemoryEveningCheckInRepository& checkIns)
    : m_chats{chats}, m_moods{moods}, m_profiles{profiles}, m_checkIns{checkIns}
{
}

void InMemoryTransactionRunner::transaction(const std::function<void()>& operation)
{
    if (!operation) {
        throw std::invalid_argument{"Не задана операция транзакции"};
    }
    auto chats = m_chats;
    auto moods = m_moods;
    auto profiles = m_profiles;
    auto checkIns = m_checkIns;
    const auto previouslyActive = m_active;
    m_active = true;
    try {
        operation();
        if (m_failCommit) {
            m_failCommit = false;
            throw std::runtime_error{"Ошибка фиксации тестовой транзакции"};
        }
        m_active = previouslyActive;
    } catch (...) {
        m_chats = std::move(chats);
        m_moods = std::move(moods);
        m_profiles = std::move(profiles);
        m_checkIns = std::move(checkIns);
        m_active = previouslyActive;
        throw;
    }
}

bool InMemoryTransactionRunner::isActive() const
{
    return m_active;
}

void InMemoryTransactionRunner::failNextCommit()
{
    m_failCommit = true;
}

}  // namespace msa::core

