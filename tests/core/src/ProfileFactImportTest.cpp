#include "InMemoryUserProfileRepository.h"

#include "msa/core/UserProfileFactSelector.h"
#include "msa/core/UserProfileService.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

[[nodiscard]] ProfileFactSource source()
{
    return ProfileFactSource{.chatIdentifier = 7, .messageIdentifier = 42, .moodIdentifier = 9};
}

TEST(ProfileFactImportTest, ImportsWholeBatchWithStableIdsAndSourceWithoutAutomaticTags)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    const auto result = service.rememberExtractedFacts(
        {{.text = "  Начала работать\nразработчиком 🙂  "}, {.text = "Готовлюсь к переезду"}}, source());
    ASSERT_TRUE(std::holds_alternative<std::vector<UserProfileFact>>(result));
    const auto added = std::get<std::vector<UserProfileFact>>(result);
    ASSERT_EQ(added.size(), 2U);
    EXPECT_EQ(added.front().text, "Начала работать\nразработчиком 🙂");
    EXPECT_EQ(added.front().identifier, 1);
    EXPECT_EQ(added.back().identifier, 2);
    EXPECT_EQ(added.front().source, source());
    EXPECT_TRUE(added.front().tags.empty());
    EXPECT_FALSE(added.front().alwaysInclude);
    UserProfileService restored{repository};
    EXPECT_EQ(restored.profile().facts, added);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(restored.rememberFact("Ручной факт")));
    EXPECT_EQ(restored.profile().facts.back().identifier, 3);
    EXPECT_FALSE(restored.profile().facts.back().source);
}

TEST(ProfileFactImportTest, SkipsExactAutomaticDuplicatesButKeepsSimilarAndManualFacts)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Люблю чай")));
    const auto result = service.rememberExtractedFacts(
        {{.text = "  Люблю чай\n"}, {.text = "Люблю зелёный чай"}, {.text = "Люблю зелёный чай"}}, source());
    const auto added = std::get<std::vector<UserProfileFact>>(result);
    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(added.front().text, "Люблю зелёный чай");
    EXPECT_EQ(service.profile().facts.size(), 2U);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Люблю чай")));
    EXPECT_EQ(service.profile().facts.size(), 3U);
}

TEST(ProfileFactImportTest, RevisionKeepsOldSourceAndMetadataAndExcludesObsoleteVersion)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Работаю в A", {"работа"}, true)));
    const auto before = service.profile().facts.front();
    const auto result = service.rememberExtractedFacts(
        {{.text = "Работаю в B", .replacesFactIdentifier = before.identifier}}, source());
    const auto added = std::get<std::vector<UserProfileFact>>(result);
    ASSERT_EQ(added.size(), 1U);
    const auto snapshot = service.profile();
    ASSERT_EQ(snapshot.facts.size(), 2U);
    EXPECT_EQ(snapshot.facts.front().text, before.text);
    EXPECT_EQ(snapshot.facts.front().source, before.source);
    EXPECT_EQ(snapshot.facts.front().supersededByFactIdentifier, added.front().identifier);
    EXPECT_EQ(added.front().replacesFactIdentifier, before.identifier);
    EXPECT_EQ(added.front().tags, before.tags);
    EXPECT_TRUE(added.front().alwaysInclude);
    EXPECT_EQ(added.front().source, source());
    const UserProfileFactSelector selector;
    EXPECT_EQ(selector.select(snapshot, "работа"), (std::vector<std::string>{"Работаю в B"}));
    EXPECT_EQ(selector.selectFacts(snapshot, "").front().identifier, added.front().identifier);
    EXPECT_EQ(service.profile().facts, snapshot.facts);
}

TEST(ProfileFactImportTest, OldTextDoesNotReactivateWithoutExplicitRevisionButCanReturnAsNewVersion)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Люблю чай", {}, true)));
    static_cast<void>(service.rememberExtractedFacts({{.text = "Люблю кофе", .replacesFactIdentifier = 1}}, source()));
    EXPECT_TRUE(std::get<std::vector<UserProfileFact>>(
        service.rememberExtractedFacts({{.text = "Люблю чай"}}, source())).empty());
    EXPECT_EQ(service.profile().facts.size(), 2U);
    const auto result = service.rememberExtractedFacts({{.text = "Люблю чай", .replacesFactIdentifier = 2}}, source());
    const auto added = std::get<std::vector<UserProfileFact>>(result);
    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(added.front().identifier, 3);
    EXPECT_EQ(added.front().replacesFactIdentifier, 2);
    EXPECT_EQ(service.profile().facts[1].supersededByFactIdentifier, 3);
    EXPECT_EQ(service.profile().facts.size(), 3U);
}

TEST(ProfileFactImportTest, InvalidBatchDoesNotSaveEarlierAdditionsOrRevisionMarks)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Работаю в A")));
    const auto before = service.profile();
    const std::vector<std::vector<ProfileFactProposal>> batches{
        {{.text = "Работаю в B", .replacesFactIdentifier = 1}, {.text = "\xC2\xA0"}},
        {{.text = "Новый факт"}, {.text = "Замена", .replacesFactIdentifier = 99}},
        {{.text = "Замена", .replacesFactIdentifier = 0}},
        {{.text = "Замена", .replacesFactIdentifier = -1}},
        {{.text = "Работаю в B", .replacesFactIdentifier = 1},
            {.text = "Работаю в C", .replacesFactIdentifier = 1}}};
    for (const auto& batch : batches) {
        EXPECT_TRUE(service.validateExtractedFacts(batch));
        EXPECT_TRUE(std::holds_alternative<UserProfileRejection>(service.rememberExtractedFacts(batch, source())));
        EXPECT_EQ(service.profile().facts, before.facts);
    }
    EXPECT_FALSE(service.validateExtractedFacts({{.text = "Новый факт"}}));
    EXPECT_EQ(service.profile().facts, before.facts);
}

TEST(ProfileFactImportTest, RejectsReplacementOfObsoleteFactAndAmbiguousExistingActiveText)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Работаю в A")));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Работаю в B")));
    const auto before = service.profile();
    EXPECT_TRUE(std::holds_alternative<UserProfileRejection>(service.rememberExtractedFacts(
        {{.text = "Работаю в B", .replacesFactIdentifier = 1}}, source())));
    EXPECT_EQ(service.profile().facts, before.facts);
    static_cast<void>(service.rememberExtractedFacts({{.text = "Работаю в C", .replacesFactIdentifier = 1}}, source()));
    const auto revised = service.profile();
    EXPECT_TRUE(std::holds_alternative<UserProfileRejection>(service.rememberExtractedFacts(
        {{.text = "Работаю в D", .replacesFactIdentifier = 1}}, source())));
    EXPECT_EQ(service.profile().facts, revised.facts);
    EXPECT_TRUE(std::get<std::vector<UserProfileFact>>(service.rememberExtractedFacts(
        {{.text = "Работаю в C", .replacesFactIdentifier = 3}}, source())).empty());
}

/// @brief Проверяет отсутствие лишних сохранений и проброс ошибки до изменения данных.
class CountingProfileRepository final : public UserProfileRepository {
public:
    [[nodiscard]] UserProfile load() const override;
    void save(const UserProfile& profile) override;
    [[nodiscard]] int saves() const;
    void failSaves();

private:
    UserProfile m_profile{};
    int m_saves{0};
    bool m_fail{false};
};

UserProfile CountingProfileRepository::load() const
{
    return m_profile;
}

void CountingProfileRepository::save(const UserProfile& profile)
{
    if (m_fail) {
        throw std::runtime_error{"Ошибка сохранения профиля"};
    }
    ++m_saves;
    m_profile = profile;
}

int CountingProfileRepository::saves() const
{
    return m_saves;
}

void CountingProfileRepository::failSaves()
{
    m_fail = true;
}

TEST(ProfileFactImportTest, SavesValidBatchOnceAndSkipsEmptyOrDuplicateWrites)
{
    CountingProfileRepository repository;
    UserProfileService service{repository};
    static_cast<void>(service.rememberExtractedFacts({{.text = "Первый"}, {.text = "Второй"}}, source()));
    EXPECT_EQ(repository.saves(), 1);
    static_cast<void>(service.rememberExtractedFacts({{.text = "Первый"}}, source()));
    static_cast<void>(service.rememberExtractedFacts({}, source()));
    EXPECT_EQ(repository.saves(), 1);
    EXPECT_FALSE(service.validateExtractedFacts({{.text = "Третий"}}));
    EXPECT_EQ(repository.saves(), 1);
    repository.failSaves();
    EXPECT_THROW(static_cast<void>(service.rememberExtractedFacts({{.text = "Третий"}}, source())), std::runtime_error);
    EXPECT_EQ(service.profile().facts.size(), 2U);
}

TEST(ProfileFactImportTest, RejectsInvalidSourceAndIdExhaustionWithoutSaving)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    for (const auto& invalid : std::vector<ProfileFactSource>{{},
        {.chatIdentifier = 1, .messageIdentifier = 0},
        {.chatIdentifier = 1, .messageIdentifier = 2, .moodIdentifier = 0}}) {
        EXPECT_TRUE(std::holds_alternative<UserProfileRejection>(
            service.rememberExtractedFacts({{.text = "Факт"}}, invalid)));
        EXPECT_TRUE(service.profile().facts.empty());
    }
    repository.save(UserProfile{.facts = {{.text = "Последний ID",
        .identifier = std::numeric_limits<std::int64_t>::max()}}});
    EXPECT_THROW(static_cast<void>(service.rememberExtractedFacts({{.text = "Новый"}}, source())), std::overflow_error);
    EXPECT_EQ(service.profile().facts.size(), 1U);
}

}  // namespace
}  // namespace msa::core

