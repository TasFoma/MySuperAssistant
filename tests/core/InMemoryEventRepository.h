#pragma once

#include "InMemoryStore.h"

#include "msa/core/EventRepository.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Тестовое хранилище Event без файлов и внешних сервисов.
class InMemoryEventRepository final : public EventRepository {
public:
    /// @brief Добавляет запись и возвращает назначенный ID.
    [[nodiscard]] std::int64_t add(const Event& item) override;
    /// @brief Возвращает копию записи или std::nullopt.
    [[nodiscard]] std::optional<Event> find(std::int64_t identifier) const override;
    /// @brief Возвращает копии всех записей.
    [[nodiscard]] std::vector<Event> all() const override;
    /// @brief Заменяет существующую запись.
    [[nodiscard]] bool update(const Event& item) override;
    /// @brief Удаляет запись.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    InMemoryStore<Event> m_items{};
};

}  // namespace msa::core

