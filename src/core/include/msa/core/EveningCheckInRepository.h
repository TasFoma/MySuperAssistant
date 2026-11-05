#pragma once

#include <chrono>
#include <optional>

namespace msa::core {

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
};

}  // namespace msa::core

