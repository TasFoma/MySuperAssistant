#include "msa/core/ClientPreferences.h"

#include "Utf8Text.h"

#include <algorithm>

namespace msa::core
{
bool validClientPreferences(const ClientPreferences& preferences)
{
    const auto& host = preferences.host;
    const auto& token = preferences.token;
    const auto invalidHostCharacter = [](unsigned char value) { return value <= 0x20 || value == 0x7f; };

    return !host.empty() && host.find('\0') == std::string::npos && host.find('/') == std::string::npos &&
           !std::ranges::any_of(host, invalidHostCharacter) && preferences.port != 0 &&
           token.find('\0') == std::string::npos && !text::trim(token).empty();
}
} // namespace msa::core
