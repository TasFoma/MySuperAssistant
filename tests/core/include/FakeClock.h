#pragma once

#include "msa/core/Clock.h"

#include <chrono>

namespace msa::core {

/// @brief Часы с явно заданным временем для тестов без ожиданий.
class FakeClock final : public Clock {
public:
    /// @brief Задаёт начальный местный момент.
    explicit FakeClock(std::chrono::local_time<std::chrono::seconds> moment);
    /// @brief Возвращает установленный момент.
    [[nodiscard]] std::chrono::local_time<std::chrono::seconds> now() const override;
    /// @brief Устанавливает время, в том числе при переводе часов назад.
    void set(std::chrono::local_time<std::chrono::seconds> moment);
    /// @brief Сдвигает время на заданный интервал.
    void advance(std::chrono::seconds elapsed);

private:
    std::chrono::local_time<std::chrono::seconds> m_moment{};
};

}  // namespace msa::core

