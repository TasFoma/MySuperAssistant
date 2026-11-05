#pragma once

#include <chrono>
#include <optional>

namespace msa::core {

/// @brief Хранилище даты последнего подготовленного приветствия; ошибки — исключениями.
class GreetingRepository {
public:
    /// @brief Освобождает хранилище.
    virtual ~GreetingRepository();
    /// @brief Возвращает дату либо nullopt, если приветствий ещё не было.
    [[nodiscard]] virtual std::optional<std::chrono::year_month_day> lastDate() const = 0;
    /// @brief Заменяет сохранённую дату приветствия.
    virtual void saveDate(std::chrono::year_month_day date) = 0;
};

}  // namespace msa::core
