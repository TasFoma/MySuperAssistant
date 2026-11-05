#include "Utf8Text.h"

#include <string_view>
#include <vector>

namespace msa::core::text {

namespace {

constexpr char32_t kLatinCapitalA = U'A';
constexpr char32_t kLatinCapitalZ = U'Z';
constexpr char32_t kLatinSmallA = U'a';
constexpr char32_t kLatinSmallZ = U'z';
constexpr char32_t kCyrillicCapitalA = 0x0410;
constexpr char32_t kCyrillicCapitalYa = 0x042F;
constexpr char32_t kCyrillicSmallA = 0x0430;
constexpr char32_t kCyrillicSmallYa = 0x044F;
constexpr char32_t kCyrillicCapitalIo = 0x0401;
constexpr char32_t kCyrillicSmallIo = 0x0451;
constexpr char32_t kCaseOffset = 0x20;
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationMark = 0x80;

[[nodiscard]] bool isContinuationByte(unsigned char byte)
{
    return (byte & kContinuationMask) == kContinuationMark;
}

[[nodiscard]] std::size_t sequenceLength(unsigned char lead)
{
    if (lead < 0x80) {
        return 1;
    }
    if ((lead >> 5) == 0x6) {
        return 2;
    }
    if ((lead >> 4) == 0xE) {
        return 3;
    }
    if ((lead >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

[[nodiscard]] char32_t leadPayload(unsigned char lead, std::size_t length)
{
    switch (length) {
    case 2:
        return lead & 0x1F;
    case 3:
        return lead & 0x0F;
    case 4:
        return lead & 0x07;
    default:
        return lead;
    }
}

[[nodiscard]] std::u32string decode(std::string_view text)
{
    std::u32string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = sequenceLength(lead);
        if (index + length > text.size()) {
            length = 1;
        }
        char32_t codePoint = leadPayload(lead, length);
        for (std::size_t offset = 1; offset < length; ++offset) {
            codePoint = (codePoint << 6) | (static_cast<unsigned char>(text[index + offset]) & 0x3F);
        }
        result.push_back(codePoint);
        index += length;
    }
    return result;
}

void appendEncoded(std::string& out, char32_t codePoint)
{
    constexpr char32_t kOneByteLimit = 0x80;
    constexpr char32_t kTwoByteLimit = 0x800;
    constexpr char32_t kThreeByteLimit = 0x10000;
    constexpr int kSixBits = 6;

    if (codePoint < kOneByteLimit) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < kTwoByteLimit) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> kSixBits)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < kThreeByteLimit) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> (kSixBits * 2))));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> kSixBits) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> (kSixBits * 3))));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> (kSixBits * 2)) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> kSixBits) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

[[nodiscard]] std::string encode(const std::u32string& codePoints)
{
    std::string result;
    for (const char32_t codePoint : codePoints) {
        appendEncoded(result, codePoint);
    }
    return result;
}

[[nodiscard]] char32_t lowerCodePoint(char32_t codePoint)
{
    if (codePoint >= kLatinCapitalA && codePoint <= kLatinCapitalZ) {
        return codePoint + kCaseOffset;
    }
    if (codePoint >= kCyrillicCapitalA && codePoint <= kCyrillicCapitalYa) {
        return codePoint + kCaseOffset;
    }
    if (codePoint == kCyrillicCapitalIo) {
        return kCyrillicSmallIo;
    }
    return codePoint;
}

[[nodiscard]] char32_t upperCodePoint(char32_t codePoint)
{
    if (codePoint >= kLatinSmallA && codePoint <= kLatinSmallZ) {
        return codePoint - kCaseOffset;
    }
    if (codePoint >= kCyrillicSmallA && codePoint <= kCyrillicSmallYa) {
        return codePoint - kCaseOffset;
    }
    if (codePoint == kCyrillicSmallIo) {
        return kCyrillicCapitalIo;
    }
    return codePoint;
}

}  // namespace

std::size_t charCount(std::string_view text)
{
    std::size_t count = 0;
    for (const char symbol : text) {
        if (!isContinuationByte(static_cast<unsigned char>(symbol))) {
            ++count;
        }
    }
    return count;
}

std::string trim(std::string_view text)
{
    constexpr std::u32string_view kWhitespace = U"\t\n\v\f\r \u0085\u00A0\u1680"
        U"\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A"
        U"\u2028\u2029\u202F\u205F\u3000";
    const auto codePoints = decode(text);
    const auto first = codePoints.find_first_not_of(kWhitespace);
    if (first == std::u32string::npos) {
        return {};
    }
    const auto last = codePoints.find_last_not_of(kWhitespace);

    return encode(codePoints.substr(first, last - first + 1));
}

std::string toLower(std::string_view text)
{
    std::u32string codePoints = decode(text);
    for (char32_t& codePoint : codePoints) {
        codePoint = lowerCodePoint(codePoint);
    }
    return encode(codePoints);
}

std::string upperFirst(std::string_view text)
{
    std::u32string codePoints = decode(text);
    if (!codePoints.empty()) {
        codePoints.front() = upperCodePoint(codePoints.front());
    }
    return encode(codePoints);
}

std::string lowerFirst(std::string_view text)
{
    std::u32string codePoints = decode(text);
    if (!codePoints.empty()) {
        codePoints.front() = lowerCodePoint(codePoints.front());
    }
    return encode(codePoints);
}

}  // namespace msa::core::text
