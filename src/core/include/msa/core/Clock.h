#pragma once

#include <chrono>

namespace msa::core {

/// @brief Источник местного времени сервера для предметной логики.
class Clock {
public:
    /// @brief Освобождает источник времени.
    virtual ~Clock();

    /// @brief Возвращает местный момент с точностью до секунд.
    [[nodiscard]] virtual std::chrono::local_time<std::chrono::seconds> now() const = 0;
};

}  // namespace msa::core

