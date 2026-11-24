#include "msa/core/SportLoad.h"

#include <gtest/gtest.h>

using namespace msa::core;
TEST(SportLoadTest, ParsesExactlyAndKeepsMissingDistinctFromZero)
{
    EXPECT_FALSE(std::get<std::optional<std::int32_t>>(parseSportMass("")));
    EXPECT_EQ(std::get<std::optional<std::int32_t>>(parseSportMass("0,01")), 1);
    EXPECT_EQ(std::get<std::optional<std::int32_t>>(parseSportMass("1000.00")), 100000);
    EXPECT_EQ(std::get<std::optional<std::int32_t>>(parseSportMass("0")), 0);
}
TEST(SportLoadTest, RejectsAmbiguousOrOutOfRangeMassWithoutRounding)
{
    for (const auto* text : {"1.001", "1000.01", "-1", "NaN", "Infinity", "1e2", "1,", ".1", "1.2.3", "999999999999"})
    {
        EXPECT_TRUE(std::holds_alternative<SportRejection>(parseSportMass(text))) << text;
    }
}
TEST(SportLoadTest, BodyweightNeedsNoMassAndSupplementsAreExclusive)
{
    EXPECT_FALSE(validateSportLoad(BodyweightLoad{}, true));
    EXPECT_FALSE(validateSportLoad(BodyweightLoad{100, std::nullopt}, true));
    EXPECT_TRUE(validateSportLoad(BodyweightLoad{100, 100}));
    EXPECT_TRUE(validateSportLoad(BodyweightLoad{-1, std::nullopt}));
}
TEST(SportLoadTest, EquipmentMassAndQuantityAreValidated)
{
    EXPECT_FALSE(validateSportLoad(DumbbellLoad{1, 1}, true));
    EXPECT_FALSE(validateSportLoad(DumbbellLoad{100000, 2}, true));
    EXPECT_TRUE(validateSportLoad(DumbbellLoad{100, 3}));
    EXPECT_TRUE(validateSportLoad(BarbellLoad{}, true));
    EXPECT_FALSE(validateSportLoad(BarbellLoad{}));
    EXPECT_TRUE(validateSportLoad(BarbellLoad{0}, true));
}
