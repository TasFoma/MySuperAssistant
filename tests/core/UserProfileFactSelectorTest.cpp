#include "InMemoryUserProfileRepository.h"

#include "msa/core/LlmContextBuilder.h"
#include "msa/core/UserProfileFactSelector.h"
#include "msa/core/UserProfileService.h"

#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

TEST(UserProfileFactSelectorTest, RanksDistinctMatchesAndKeepsInsertionOrderForTies)
{
    const UserProfile profile{.facts = {
        {.text = "Чай чай ЧАЙ", .tags = {"чай", "ЧАЙ"}},
        {.text = "Кофе и чай"}, {.text = "Зелёный чай"}, {.text = "Музыка"}}};
    const UserProfileFactSelector selector;
    EXPECT_EQ(selector.select(profile, "ЧАЙ, кофе! чай"),
        (std::vector<std::string>{"Кофе и чай", "Чай чай ЧАЙ", "Зелёный чай"}));
}

TEST(UserProfileFactSelectorTest, UsesManualTagsWithoutAddingThemToReturnedText)
{
    const UserProfile profile{.facts = {
        {.text = "Учусь писать программы", .tags = {"Работа", "C++ Qt"}},
        {.text = "Люблю прогулки", .tags = {"Спорт"}}}};
    const UserProfileFactSelector selector;
    EXPECT_EQ(selector.select(profile, "QT: работа"),
        (std::vector<std::string>{"Учусь писать программы"}));
}

TEST(UserProfileFactSelectorTest, PrioritizesAlwaysIncludeRegardlessOfTopicAndScore)
{
    const UserProfile profile{.facts = {
        {.text = "Чай кофе"}, {.text = "Отвечай кратко", .alwaysInclude = true},
        {.text = "Чай кофе", .alwaysInclude = true}, {.text = "Чай"}}};
    const UserProfileFactSelector selector;
    EXPECT_EQ(selector.select(profile, "Чай кофе"),
        (std::vector<std::string>{"Отвечай кратко", "Чай кофе", "Чай кофе", "Чай"}));
}

TEST(UserProfileFactSelectorTest, EmptyOrUnmatchedQueryReturnsOnlyAlwaysInclude)
{
    const UserProfile profile{.facts = {
        {.text = "Люблю чай"}, {.text = "Отвечай кратко", .alwaysInclude = true}}};
    const UserProfileFactSelector selector;
    for (const auto& question : std::vector<std::string>{"", " \n", "🙂!?", "Программирование"}) {
        EXPECT_EQ(selector.select(profile, question), (std::vector<std::string>{"Отвечай кратко"}));
        EXPECT_TRUE(selector.select(UserProfile{}, question).empty());
    }
}

TEST(UserProfileFactSelectorTest, MatchesWholeWordsWithLatinCyrillicDigitsAndCombiningMarks)
{
    const UserProfile profile{.facts = {{.text = "Ёлка"}, {.text = "Qt6"}, {.text = "2026"},
        {.text = "е\u0301"}, {.text = "кот"}, {.text = "котик"}, {.text = "кота"}}};
    const UserProfileFactSelector selector;
    EXPECT_EQ(selector.select(profile, "ёлка\xC2\xA0QT6—2026, Е\u0301 кот"),
        (std::vector<std::string>{"Ёлка", "Qt6", "2026", "е\u0301", "кот"}));
    // Синонимы и склонения не угадываются; е и ё не объединяются.
    EXPECT_TRUE(selector.select(profile, "елка кошка").empty());
}

TEST(UserProfileFactSelectorTest, PreservesFullTextsDuplicatesAndStoredMetadata)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    const std::string fact = "Чай\n  с молоком🙂";
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact(fact, {"Напитки"})));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact(fact, {"Напитки"})));
    const auto before = service.profile();
    const UserProfileFactSelector selector;
    auto selected = selector.select(service.profile(), "напитки");
    EXPECT_EQ(selected, (std::vector<std::string>{fact, fact}));
    selected.front().clear();
    EXPECT_EQ(service.profile().facts, before.facts);
    EXPECT_EQ(selector.select(service.profile(), "напитки"), (std::vector<std::string>{fact, fact}));
}

TEST(UserProfileFactSelectorTest, BuilderLimitsRankedFactsWithoutDeletingStoredData)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Я🙂", {}, true)));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact("Чай\nс молоком")));
    const auto before = service.profile();
    const UserProfileFactSelector selector;
    const LlmContextInput input{.systemPrompt = "S", .currentQuestion = "чай",
        .selectedFacts = selector.select(before, "чай")};
    const LlmContextBuilder builder;
    // S + вопрос (4), заголовок (16), маркер (3), факт Я🙂 (2).
    constexpr std::size_t kBudget = 25;
    const auto result = builder.build(input, kBudget);
    ASSERT_TRUE(std::holds_alternative<LlmContext>(result));
    const auto context = std::get<LlmContext>(result);
    EXPECT_EQ(context.messages.front().text, "S\n\nФакты профиля:\n• Я🙂");
    EXPECT_EQ(context.usedChars, kBudget);
    EXPECT_EQ(context.omittedFacts, 1U);
    EXPECT_EQ(context.messages.back().text, "чай");
    const auto smaller = std::get<LlmContext>(builder.build(input, kBudget - 1));
    EXPECT_EQ(smaller.messages.front().text, "S");
    EXPECT_EQ(smaller.omittedFacts, 2U);
    EXPECT_EQ(service.profile().facts, before.facts);
}

}  // namespace
}  // namespace msa::core

