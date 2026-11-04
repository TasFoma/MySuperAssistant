#pragma once

#include "msa/core/Command.h"

#include <chrono>
#include <string_view>

namespace msa::core {

/// @brief Локальный разбор русских команд без нейросети.
/// Примеры: «напомни завтра в 15:00 позвонить маме», «добавь задачу купить продукты на завтра».
class CommandParser {
public:
    /// @brief Разбирает текст команды.
    /// @param text Текст в UTF-8.
    /// @param now Текущее местное время: от него считаются «завтра», «через час» и т. п.
    /// @return Распознанная команда или NoCommand.
    [[nodiscard]] Command parse(std::string_view text, std::chrono::local_time<std::chrono::seconds> now) const;
};

}  // namespace msa::core
