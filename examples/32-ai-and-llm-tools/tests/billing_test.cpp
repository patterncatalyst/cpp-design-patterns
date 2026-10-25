#include <gtest/gtest.h>

#include "billing.hpp"
#include "usage_tier.hpp"

TEST(MonthlyBillUsd, StarterIsMeteredPerRequest) {
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Starter, 500), 10.00);
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Starter, 9'999), 199.98);
}

TEST(MonthlyBillUsd, GrowthAndScaleAreFlatFeesRegardlessOfVolume) {
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Growth, 10'000), 99.00);
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Growth, 99'999), 99.00);
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Scale, 100'000), 499.00);
    EXPECT_DOUBLE_EQ(MonthlyBillUsd(UsageTier::Scale, 5'000'000), 499.00);
}

TEST(MonthlyBillUsd, TheGrowthBoundaryBugCostsASpecificCustomerOneHundredOneDollars) {
    const double correct_bill = MonthlyBillUsd(UsageTier::Growth, 10'000);
    const double misclassified_bill = MonthlyBillUsd(UsageTier::Starter, 10'000);
    EXPECT_DOUBLE_EQ(misclassified_bill - correct_bill, 101.00);
}
