// Дымовой тест: проверяет, что тестовая цель собирается с C++20 и запускается через ctest.
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

TEST(CoreSmokeTest, SortedVectorIsSorted)
{
    std::vector<int> numbers{3, 1, 2};

    std::ranges::sort(numbers);

    EXPECT_TRUE(std::ranges::is_sorted(numbers));
}

}  // namespace
