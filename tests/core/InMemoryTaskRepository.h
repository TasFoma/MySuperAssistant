#pragma once

#include "InMemoryStore.h"

#include "msa/core/TaskRepository.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace msa::core {

/// @brief Тестовое хранилище Task без файлов и внешних сервисов.
class InMemoryTaskRepository final : public TaskRepository {
public:
    /// @brief Добавляет запись и возвращает назначенный ID.
    [[nodiscard]] std::int64_t add(const Task& item) override;
    /// @brief Возвращает копию записи или std::nullopt.
    [[nodiscard]] std::optional<Task> find(std::int64_t identifier) const override;
    /// @brief Возвращает копии всех записей.
    [[nodiscard]] std::vector<Task> all() const override;
    /// @brief Заменяет существующую запись.
    [[nodiscard]] bool update(const Task& item) override;
    /// @brief Удаляет запись.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    InMemoryStore<Task> m_items{};
};

}  // namespace msa::core

