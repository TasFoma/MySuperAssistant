#include "Pcre2Regex.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace msa::core::detail {

namespace {

constexpr std::size_t kErrorBufferSize = 256;
constexpr std::size_t kContinuationMask = 0xC0;
constexpr std::size_t kContinuationMark = 0x80;

/// @brief Сдвигает смещение на начало следующего символа UTF-8.
[[nodiscard]] std::size_t nextCharBoundary(std::string_view subject, std::size_t offset)
{
    std::size_t next = offset + 1;
    while (next < subject.size()
           && (static_cast<unsigned char>(subject[next]) & kContinuationMask) == kContinuationMark) {
        ++next;
    }
    return next;
}

/// @brief Ищет совпадение, начиная с байтового смещения from. Не владеет кодом шаблона.
[[nodiscard]] std::optional<RegexMatch> searchFrom(const pcre2_code* code, std::string_view subject,
                                                   std::size_t from)
{
    pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code, nullptr);
    const int result = pcre2_match(code, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(),
                                   from, 0, matchData, nullptr);
    if (result < 0) {
        pcre2_match_data_free(matchData);
        return std::nullopt;
    }

    // Необязательные хвостовые группы могут не попасть в ovector, поэтому считаем по шаблону.
    std::uint32_t captureCount = 0;
    pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &captureCount);
    const std::size_t groupCount = static_cast<std::size_t>(captureCount) + 1;
    const auto returnedCount = static_cast<std::size_t>(result);

    RegexMatch found;
    const PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
    found.start = ovector[0];
    found.end = ovector[1];
    found.groups.reserve(groupCount);
    for (std::size_t group = 0; group < groupCount; ++group) {
        const PCRE2_SIZE begin = ovector[group * 2];
        const PCRE2_SIZE end = ovector[group * 2 + 1];
        if (group >= returnedCount || begin == PCRE2_UNSET || end == PCRE2_UNSET) {
            found.groups.emplace_back();
        } else {
            found.groups.emplace_back(subject.substr(begin, end - begin));
        }
    }

    pcre2_match_data_free(matchData);
    return found;
}

}  // namespace

Pcre2Regex::Pcre2Regex(std::string_view pattern)
{
    int errorCode = 0;
    PCRE2_SIZE errorOffset = 0;
    const std::uint32_t options = PCRE2_UTF | PCRE2_UCP | PCRE2_CASELESS;

    m_code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), options,
                           &errorCode, &errorOffset, nullptr);
    if (m_code == nullptr) {
        std::array<PCRE2_UCHAR, kErrorBufferSize> message{};
        pcre2_get_error_message(errorCode, message.data(), message.size());
        throw std::logic_error(std::string("Некорректный шаблон PCRE2: ")
                               + reinterpret_cast<const char*>(message.data()));
    }
}

Pcre2Regex::~Pcre2Regex()
{
    pcre2_code_free(m_code);
}

std::optional<RegexMatch> Pcre2Regex::match(std::string_view subject) const
{
    return searchFrom(m_code, subject, 0);
}

std::vector<RegexMatch> Pcre2Regex::matchAll(std::string_view subject) const
{
    std::vector<RegexMatch> all;
    std::size_t offset = 0;
    while (offset <= subject.size()) {
        auto found = searchFrom(m_code, subject, offset);
        if (!found) {
            break;
        }
        // Пустое совпадение сдвигаем на следующий символ, иначе цикл не закончится.
        offset = found->end > found->start ? found->end : nextCharBoundary(subject, found->end);
        all.push_back(std::move(*found));
    }
    return all;
}

}  // namespace msa::core::detail
