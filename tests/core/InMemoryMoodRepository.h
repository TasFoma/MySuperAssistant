#pragma once

#include "InMemoryStore.h"

#include "msa/core/MoodRepository.h"

#include <cstdint>
#include <vector>

namespace msa::core {

/// @brief Тестовый дневник настроения без файлов и внешних сервисов.
class InMemoryMoodRepository final : public MoodRepository {
public:
    /// @brief Сохраняет копию и возвращает назначенный ID.
    [[nodiscard]] std::int64_t add(const MoodEntry& entry) override;
    /// @brief Возвращает независимые копии всех записей.
    [[nodiscard]] std::vector<MoodEntry> all() const override;

private:
    InMemoryStore<MoodEntry> m_entries{};
};

}  // namespace msa::core

