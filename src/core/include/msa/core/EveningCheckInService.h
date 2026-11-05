#pragma once

#include "msa/core/Clock.h"
#include "msa/core/EveningCheckInRepository.h"

#include <optional>
#include <string>

namespace msa::core {

/// @brief Подготавливает вечерний опрос раз за местный календарный день с 20:00.
class EveningCheckInService {
public:
    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    EveningCheckInService(const Clock& clock, EveningCheckInRepository& repository);
    /// @brief Возвращает текст при первой проверке с 20:00 до конца текущего дня.
    /// До 20:00 и после подготовки в этот день возвращает nullopt. Пропущенные дни
    /// не воспроизводятся. Дата сохраняется до возврата; ошибки пробрасываются.
    /// Доставка, обработка ответа и запись настроения находятся вне сервиса.
    [[nodiscard]] std::optional<std::string> checkInIfNeeded();

private:
    const Clock& m_clock;
    EveningCheckInRepository& m_repository;
};

}  // namespace msa::core

