#pragma once

#include "InMemoryStore.h"

#include "msa/core/ReminderRepository.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Тестовое хранилище Reminder без файлов и внешних сервисов.
class InMemoryReminderRepository final : public ReminderRepository {
public:
    /// @brief Добавляет запись и возвращает назначенный ID.
    [[nodiscard]] std::int64_t add(const Reminder& item) override;
    /// @brief Возвращает копию записи или std::nullopt.
    [[nodiscard]] std::optional<Reminder> find(std::int64_t identifier) const override;
    /// @brief Возвращает копии всех записей.
    [[nodiscard]] std::vector<Reminder> all() const override;
    /// @brief Заменяет существующую запись.
    [[nodiscard]] bool update(const Reminder& item) override;
    /// @brief Удаляет запись.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    InMemoryStore<Reminder> m_items{};
};

}  // namespace msa::core

