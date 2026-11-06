#include "msa/core/UserProfileService.h"

#include "Utf8Text.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace msa::core {
namespace {

[[nodiscard]] std::int64_t nextIdentifier(const UserProfile& profile)
{
    std::int64_t maximum = 0;
    std::set<std::int64_t> identifiers;
    for (const auto& fact : profile.facts) {
        if (fact.identifier <= 0 || !identifiers.insert(fact.identifier).second) {
            throw std::runtime_error{"В профиле обнаружены неверные или повторные ID фактов"};
        }
        maximum = std::max(maximum, fact.identifier);
    }
    if (maximum == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error{"Исчерпаны идентификаторы фактов профиля"};
    }

    return maximum + 1;
}

[[nodiscard]] ProfileFactImportResult applyProposals(UserProfile& profile,
    const std::vector<ProfileFactProposal>& proposals, std::optional<ProfileFactSource> source)
{
    std::vector<UserProfileFact> added;
    std::set<std::int64_t> replaced;
    std::set<std::int64_t> originalIdentifiers;
    for (const auto& fact : profile.facts) {
        originalIdentifiers.insert(fact.identifier);
    }
    for (const auto& proposal : proposals) {
        const auto factText = text::trim(proposal.text);
        if (factText.empty()) {
            return UserProfileRejection{"Модель предложила пустой факт."};
        }
        auto previous = profile.facts.end();
        if (proposal.replacesFactIdentifier) {
            const auto target = *proposal.replacesFactIdentifier;
            if (target <= 0 || !originalIdentifiers.contains(target) || !replaced.insert(target).second) {
                return UserProfileRejection{"Укажите существующий факт для однократного исправления."};
            }
            previous = std::ranges::find(profile.facts, target, &UserProfileFact::identifier);
            if (previous->supersededByFactIdentifier) {
                return UserProfileRejection{"Исправляемая версия факта уже неактуальна."};
            }
            if (previous->text == factText) {
                continue;
            }
            const auto duplicate = std::ranges::find_if(profile.facts,
                [&factText](const UserProfileFact& fact) {
                    return !fact.supersededByFactIdentifier && fact.text == factText;
                });
            if (duplicate != profile.facts.end()) {
                return UserProfileRejection{"Такой актуальный факт уже есть; уточните исправление."};
            }
        } else if (std::ranges::any_of(profile.facts, [&factText](const UserProfileFact& fact) {
                       return fact.text == factText;
                   })) {
            // Повтор не должен заново активировать уже заменённую старую версию.
            continue;
        }
        UserProfileFact entry{.text = factText, .identifier = nextIdentifier(profile), .source = source,
            .replacesFactIdentifier = proposal.replacesFactIdentifier};
        if (previous != profile.facts.end()) {
            entry.tags = previous->tags;
            entry.alwaysInclude = previous->alwaysInclude;
            previous->supersededByFactIdentifier = entry.identifier;
        }
        profile.facts.push_back(entry);
        added.push_back(entry);
    }

    return added;
}

}  // namespace

UserProfileService::UserProfileService(UserProfileRepository& repository)
    : m_repository{repository}
{
}

UserProfileWriteResult UserProfileService::rememberFact(const std::string& fact,
    const std::vector<std::string>& tags, bool alwaysInclude)
{
    const auto trimmedFact = text::trim(fact);
    if (trimmedFact.empty()) {
        return UserProfileRejection{"Введите непустой факт о себе."};
    }
    UserProfileFact entry{.text = trimmedFact, .alwaysInclude = alwaysInclude};
    for (const auto& tag : tags) {
        const auto trimmedTag = text::trim(tag);
        if (trimmedTag.empty()) {
            return UserProfileRejection{"Введите непустой тег или уберите его."};
        }
        entry.tags.push_back(trimmedTag);
    }
    auto savedProfile = m_repository.load();
    entry.identifier = nextIdentifier(savedProfile);
    savedProfile.facts.push_back(entry);
    m_repository.save(savedProfile);

    return std::monostate{};
}

UserProfile UserProfileService::profile() const
{
    return m_repository.load();
}

std::optional<UserProfileRejection> UserProfileService::validateExtractedFacts(
    const std::vector<ProfileFactProposal>& proposals) const
{
    if (proposals.empty()) {
        return std::nullopt;
    }
    auto snapshot = m_repository.load();
    const auto result = applyProposals(snapshot, proposals, std::nullopt);
    if (const auto* rejection = std::get_if<UserProfileRejection>(&result)) {
        return *rejection;
    }

    return std::nullopt;
}

ProfileFactImportResult UserProfileService::rememberExtractedFacts(
    const std::vector<ProfileFactProposal>& proposals, const ProfileFactSource& source)
{
    if (proposals.empty()) {
        return std::vector<UserProfileFact>{};
    }
    if (source.chatIdentifier <= 0 || source.messageIdentifier <= 0
        || (source.moodIdentifier && *source.moodIdentifier <= 0)) {
        return UserProfileRejection{"Укажите сохранённое исходное сообщение и допустимую запись дневника."};
    }
    auto snapshot = m_repository.load();
    auto result = applyProposals(snapshot, proposals, source);
    if (const auto* added = std::get_if<std::vector<UserProfileFact>>(&result); added && !added->empty()) {
        m_repository.save(snapshot);
    }

    return result;
}

}  // namespace msa::core

