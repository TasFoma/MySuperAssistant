#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace msa::core {

/// @brief Ожидаемый первый ответ на сохранённый опрос в одном чате.
struct PendingEveningCheckIn {
    /// @brief Чат, в котором опубликован опрос.
    std::int64_t chatIdentifier{0};
    /// @brief ID сообщения опроса для поиска первого следующего User.
    std::int64_t messageIdentifier{0};
    /// @brief Местная дата опроса; в другую дату ожидание недействительно.
    std::chrono::year_month_day date{};
};

/// @brief Хранилище даты последнего подготовленного вечернего опроса.
/// Технические ошибки передаются исключениями без Qt-типов.
class EveningCheckInRepository {
public:
    /// @brief Освобождает хранилище.
    virtual ~EveningCheckInRepository();
    /// @brief Возвращает дату либо nullopt, если опросов ещё не было.
    [[nodiscard]] virtual std::optional<std::chrono::year_month_day> lastDate() const = 0;
    /// @brief Заменяет сохранённую дату вечернего опроса.
    virtual void saveDate(std::chrono::year_month_day date) = 0;
    /// @brief Возвращает копию ожидания либо nullopt. Дата выдачи хранится независимо.
    [[nodiscard]] virtual std::optional<PendingEveningCheckIn> pendingCheckIn() const = 0;
    /// @brief Заменяет ожидание; nullopt снимает его, не сбрасывая дату выдачи.
    virtual void savePendingCheckIn(std::optional<PendingEveningCheckIn> pending) = 0;
};

}  // namespace msa::core

