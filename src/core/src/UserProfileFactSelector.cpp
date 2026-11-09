#include "msa/core/UserProfileFactSelector.h"

#include "msa/core/detail/Pcre2Regex.h"
#include "msa/core/detail/Utf8Text.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace msa::core {
namespace {

using Words = std::set<std::string>;

[[nodiscard]] Words words(const detail::Pcre2Regex& pattern, const std::string& value)
{
    Words result;
    for (const auto& match : pattern.matchAll(value)) {
        result.insert(text::toLower(match.groups.front()));
    }

    return result;
}

struct Candidate {
    const UserProfileFact* fact{nullptr};
    std::size_t score{0};
};

}  // namespace

std::vector<std::string> UserProfileFactSelector::select(
    const UserProfile& profile, const std::string& question) const
{
    std::vector<std::string> selected;
    for (const auto& fact : selectFacts(profile, question)) {
        selected.push_back(fact.text);
    }

    return selected;
}

std::vector<UserProfileFact> UserProfileFactSelector::selectFacts(
    const UserProfile& profile, const std::string& question) const
{
    const detail::Pcre2Regex pattern{R"([\p{L}\p{N}][\p{L}\p{N}\p{M}]*)"};
    const auto queryWords = words(pattern, question);
    std::vector<Candidate> candidates;
    for (const auto& fact : profile.facts) {
        if (fact.supersededByFactIdentifier) {
            continue;
        }
        auto factWords = words(pattern, fact.text);
        for (const auto& tag : fact.tags) {
            const auto tagWords = words(pattern, tag);
            factWords.insert(tagWords.begin(), tagWords.end());
        }
        const auto score = static_cast<std::size_t>(std::ranges::count_if(queryWords,
            [&factWords](const std::string& word) { return factWords.contains(word); }));
        if (fact.alwaysInclude || score > 0) {
            candidates.push_back(Candidate{.fact = &fact, .score = score});
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (left.fact->alwaysInclude != right.fact->alwaysInclude) {
                return left.fact->alwaysInclude;
            }
            if (left.fact->alwaysInclude) {
                return false;
            }

            return left.score > right.score;
        });
    std::vector<UserProfileFact> selected;
    for (const auto& candidate : candidates) {
        selected.push_back(*candidate.fact);
    }

    return selected;
}

}  // namespace msa::core

