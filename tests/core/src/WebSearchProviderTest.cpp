#include "FakeWebSearchProvider.h"
#include "msa/core/detail/Utf8Text.h"
#include "msa/core/LlmContextBuilder.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace msa::core
{
namespace
{

TEST(WebSearchProviderTest, RejectsBlankQueryAndTrimsOnlyExplicitPhrase)
{
    FakeWebSearchProvider search;
    auto result = search.search(" \n ");
    ASSERT_TRUE(std::holds_alternative<ExternalServiceError>(result));
    EXPECT_EQ(std::get<ExternalServiceError>(result).kind, ExternalServiceErrorKind::InvalidRequest);
    EXPECT_TRUE(search.queries.empty());
    search.result = std::vector<WebSearchResult>{{"Источник", "https://example.org/", "Описание"}};
    EXPECT_TRUE(std::holds_alternative<std::vector<WebSearchResult>>(search.search("  Фраза\nс деталями  ")));
    ASSERT_EQ(search.queries.size(), 1);
    EXPECT_EQ(search.queries.front(), "Фраза\nс деталями");
}

TEST(WebSearchProviderTest, LimitsToThreeSourcesAndTruncatesSnippetByUnicodeCodePoints)
{
    FakeWebSearchProvider search;
    std::string snippet;
    for (int index = 0; index < 305; ++index)
    {
        snippet += "🙂";
    }
    search.result = std::vector<WebSearchResult>{{"Первый", "https://example.org/1", snippet},
                                                 {"Второй", "https://example.org/2", "Описание"},
                                                 {"Третий", "https://example.org/3", ""},
                                                 {"Четвёртый", "https://example.org/4", ""}};
    const auto result = search.search("Фраза");
    ASSERT_TRUE(std::holds_alternative<std::vector<WebSearchResult>>(result));
    const auto& sources = std::get<std::vector<WebSearchResult>>(result);
    ASSERT_EQ(sources.size(), 3);
    EXPECT_EQ(text::charCount(sources[0].snippet), 300);
    EXPECT_EQ(sources[0].snippet, snippet.substr(0, 300 * std::string{"🙂"}.size()));
    const auto context = webSearchContext(sources);
    EXPECT_NE(context.find("ВНЕШНИЕ ДАННЫЕ"), std::string::npos);
    EXPECT_NE(context.find("[3]"), std::string::npos);
    EXPECT_EQ(context.find("Четвёртый"), std::string::npos);
    EXPECT_NE(webSearchSourcesText(sources).find("https://example.org/3"), std::string::npos);
}

TEST(WebSearchProviderTest, ReportsEmptyOrInvalidResultsAndNeverRetries)
{
    FakeWebSearchProvider search;
    auto result = search.search("Фраза");
    ASSERT_TRUE(std::holds_alternative<ExternalServiceError>(result));
    EXPECT_EQ(std::get<ExternalServiceError>(result).kind, ExternalServiceErrorKind::NotFound);
    for (const auto& url :
         {"", "javascript:alert(1)", "https:///path", "https://user:secret@example.org/", "https://example.org/\n"})
    {
        search.result = std::vector<WebSearchResult>{{"Источник", url, "Текст"}};
        result = search.search("Фраза");
        ASSERT_TRUE(std::holds_alternative<ExternalServiceError>(result));
        EXPECT_EQ(std::get<ExternalServiceError>(result).kind, ExternalServiceErrorKind::InvalidResponse);
    }
    EXPECT_TRUE(validWebSourceUrl("HTTPS://example.org/page?q=1#section"));
    search.result = ExternalServiceError{ExternalServiceErrorKind::Unavailable, "Недоступен"};
    result = search.search("Фраза");
    EXPECT_EQ(std::get<ExternalServiceError>(result).kind, ExternalServiceErrorKind::Unavailable);
    EXPECT_EQ(search.queries.size(), 7);
}

TEST(WebSearchProviderTest, BuilderKeepsExternalDataSeparateFromInstructionsAndCurrentQuestion)
{
    const LlmContextBuilder builder;
    const LlmContextInput input{.systemPrompt = "S",
                                .currentQuestion = "Вопрос",
                                .history = {{.identifier = 1, .text = "Раньше"}},
                                .externalContext = "Данные 🙂"};
    const auto mandatory = text::charCount(input.systemPrompt + input.currentQuestion + input.externalContext);
    const auto result = builder.build(input, mandatory);
    ASSERT_TRUE(std::holds_alternative<LlmContext>(result));
    const auto& context = std::get<LlmContext>(result);
    ASSERT_EQ(context.messages.size(), 3);
    EXPECT_EQ(context.messages[0].text, "S");
    EXPECT_EQ(context.messages[1].role, LlmRole::User);
    EXPECT_EQ(context.messages[1].text, input.externalContext);
    EXPECT_EQ(context.messages[2].text, input.currentQuestion);
    EXPECT_EQ(context.usedChars, mandatory);
    EXPECT_EQ(context.omittedHistoryMessages, 1);
    EXPECT_TRUE(std::holds_alternative<LlmContextRejection>(builder.build(input, mandatory - 1)));
}

} // namespace
} // namespace msa::core
