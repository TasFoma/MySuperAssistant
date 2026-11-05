#pragma once

#include "msa/core/EventRepository.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace msa::core {

/// @brief Причина отказа операции с событием.
struct EventRejection {
    std::string reason;
};

/// @brief ID созданного события либо причина отказа.
using EventWriteResult = std::variant<std::int64_t, EventRejection>;
/// @brief Успех изменения либо причина отказа.
using EventChangeResult = std::variant<std::monostate, EventRejection>;

/// @brief Создание и изменение событий; технические исключения пробрасываются.
class EventService {
public:
    /// @brief Принимает невладеющую ссылку на репозиторий, живущий дольше сервиса.
    explicit EventService(EventRepository& repository);
    /// @brief Создаёт событие; прошедшая дата допустима, описание необязательно.
    /// Название содержит минимум одну кодовую точку, дата корректна,
    /// время отсутствует или находится в пределах 00:00–23:59, повтор допустим.
    [[nodiscard]] EventWriteResult addEvent(const std::string& title,
        std::chrono::year_month_day date, std::optional<std::chrono::minutes> time,
        const std::string& description = {}, EventRepeat repeat = EventRepeat::None);
    /// @brief Заменяет поля события с теми же проверками, сохраняя ID.
    /// Меняется вся серия повторений; отсутствующая запись возвращается как отказ.
    [[nodiscard]] EventChangeResult updateEvent(std::int64_t identifier,
        const std::string& title, std::chrono::year_month_day date,
        std::optional<std::chrono::minutes> time, const std::string& description,
        EventRepeat repeat);

private:
    EventRepository& m_repository;
};

}  // namespace msa::core

