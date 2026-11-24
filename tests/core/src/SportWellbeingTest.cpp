#include "msa/core/SportWellbeing.h"

#include <gtest/gtest.h>

using namespace msa::core;
TEST(SportWellbeingTest, MissingAndZeroPainAreIndependent)
{
    SportWellbeing value;
    EXPECT_FALSE(validateSportWellbeing(value));
    EXPECT_FALSE(value.pain);
    value.pain = 0;
    value.newPain = true;
    value.areas = {"shoulder", "knee"};
    EXPECT_FALSE(validateSportWellbeing(value));
    EXPECT_EQ(value.pain, 0);
}
TEST(SportWellbeingTest, ValidatesBoundsAndOtherArea)
{
    SportWellbeing value;
    value.wellbeing = 1;
    value.fatigue = 5;
    value.pain = 10;
    EXPECT_FALSE(validateSportWellbeing(value));
    value.pain = 11;
    EXPECT_TRUE(validateSportWellbeing(value));
    value.pain.reset();
    value.areas = {"other"};
    EXPECT_TRUE(validateSportWellbeing(value));
    value.otherArea = "Описание";
    EXPECT_FALSE(validateSportWellbeing(value));
}
