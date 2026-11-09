#include "msa/core/detail/Pcre2Regex.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace msa::core::detail {

TEST(Pcre2RegexTest, InvalidPatternThrowsDuringConstruction)
{
    EXPECT_THROW(Pcre2Regex{"("}, std::logic_error);
}

TEST(Pcre2RegexTest, SearchErrorsThrowInsteadOfReturningNoMatch)
{
    const Pcre2Regex regex{".+"};
    const std::string invalidUtf8(1, static_cast<char>(0xFF));

    EXPECT_THROW(static_cast<void>(regex.match(invalidUtf8)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(regex.matchAll(invalidUtf8)), std::runtime_error);
}

TEST(Pcre2RegexTest, NoMatchIsAnOrdinaryResult)
{
    const Pcre2Regex regex{"abc"};

    EXPECT_FALSE(regex.match("xyz").has_value());
    EXPECT_TRUE(regex.matchAll("xyz").empty());
}

}  // namespace msa::core::detail
