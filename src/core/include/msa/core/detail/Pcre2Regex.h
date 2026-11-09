#pragma once

#include <pcre2.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace msa::core::detail {

/// @brief Результат совпадения. Смещения — в байтах UTF-8 исходной строки.
struct RegexMatch {
    std::size_t start{0};
    std::size_t end{0};
    /// groups[0] — всё совпадение; несработавшая группа — пустая строка.
    std::vector<std::string> groups;
};

/// @brief RAII-обёртка над шаблоном PCRE2 с флагами UTF, UCP и без учёта регистра.
/// Совпадает с QRegularExpression, где использовались CaseInsensitiveOption и UseUnicodePropertiesOption.
class Pcre2Regex {
public:
    /// @brief Компилирует шаблон. Бросает std::logic_error: шаблоны константны, ошибка — дефект кода.
    explicit Pcre2Regex(std::string_view pattern);
    ~Pcre2Regex();

    Pcre2Regex(const Pcre2Regex&) = delete;
    Pcre2Regex& operator=(const Pcre2Regex&) = delete;
    Pcre2Regex(Pcre2Regex&&) = delete;
    Pcre2Regex& operator=(Pcre2Regex&&) = delete;

    /// @brief Первое совпадение; отсутствие — nullopt, ошибка поиска — std::runtime_error.
    [[nodiscard]] std::optional<RegexMatch> match(std::string_view subject) const;

    /// @brief Все непересекающиеся совпадения; ошибка поиска — std::runtime_error.
    [[nodiscard]] std::vector<RegexMatch> matchAll(std::string_view subject) const;

private:
    pcre2_code* m_code{nullptr};
};

}  // namespace msa::core::detail
