#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace msa::core::text {

/// @brief Количество символов (кодовых точек) в строке UTF-8.
[[nodiscard]] std::size_t charCount(std::string_view text);

/// @brief Удаляет пробельные символы Unicode по краям строки UTF-8.
[[nodiscard]] std::string trim(std::string_view text);

/// @brief Переводит в нижний регистр латиницу и кириллицу.
[[nodiscard]] std::string toLower(std::string_view text);

/// @brief Делает первую букву заглавной (латиница и кириллица).
[[nodiscard]] std::string upperFirst(std::string_view text);

/// @brief Делает первую букву строчной (латиница и кириллица).
[[nodiscard]] std::string lowerFirst(std::string_view text);

}  // namespace msa::core::text
