#pragma once

#include "msa/core/MoodEntry.h"

#include <cstdint>
#include <vector>

namespace msa::core {

/// @brief Хранилище дневника без автоматического удаления по возрасту записей.
/// Технические ошибки передаются исключениями без Qt-типов.
class MoodRepository {
public:
    /// @brief Освобождает реализацию хранилища.
    virtual ~MoodRepository();

    /// @brief Сохраняет копию записи с нулевым ID и возвращает положительный ID.
    /// Исходный объект и ранее сохранённые записи не меняются.
    [[nodiscard]] virtual std::int64_t add(const MoodEntry& entry) = 0;
    /// @brief Возвращает независимые копии всех записей без гарантии порядка.
    [[nodiscard]] virtual std::vector<MoodEntry> all() const = 0;
};

}  // namespace msa::core

