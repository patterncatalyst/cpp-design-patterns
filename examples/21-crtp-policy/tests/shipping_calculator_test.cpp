#include "shipping_calculator.hpp"

#include <gtest/gtest.h>

namespace crtp21 {
namespace {

constexpr Shipment kCartItem{/*weight_kg=*/3.5, /*declared_value_cents=*/12345};

TEST(ShippingCalculatorTest, NoRoundingAndNoSurchargeQuotesExactlyTheBaseRate) {
    const ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy> calculator(1000);

    EXPECT_EQ(calculator.quote(kCartItem), 1000);
}

TEST(ShippingCalculatorTest, FlatSurchargeThenRoundUpToNickelChangesTheQuote) {
    const ShippingCalculator<RoundUpToNickelPolicy, FlatSurchargePolicy> calculator(
        1000, RoundUpToNickelPolicy{}, FlatSurchargePolicy{153});

    // base 1000 + flat surcharge 153 = 1153, rounded up to the next nickel: 1155.
    EXPECT_EQ(calculator.quote(kCartItem), 1155);
}

TEST(ShippingCalculatorTest, DeclaredValueSurchargeThenRoundUpToNickelProducesAThirdResult) {
    const ShippingCalculator<RoundUpToNickelPolicy, DeclaredValueSurchargePolicy> calculator(1000);

    // base 1000 + 1% of 12345 declared cents (123, integer division) = 1123,
    // rounded up to the next nickel: 1125.
    EXPECT_EQ(calculator.quote(kCartItem), 1125);
}

TEST(ShippingCalculatorTest, ThreePolicyCombinationsAllProduceDifferentCorrectResults) {
    const ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy> base_only(1000);
    const ShippingCalculator<RoundUpToNickelPolicy, FlatSurchargePolicy> flat_plus_nickel(
        1000, RoundUpToNickelPolicy{}, FlatSurchargePolicy{153});
    const ShippingCalculator<RoundUpToNickelPolicy, DeclaredValueSurchargePolicy> declared_value_plus_nickel(1000);

    const Cents base_quote = base_only.quote(kCartItem);
    const Cents flat_quote = flat_plus_nickel.quote(kCartItem);
    const Cents declared_value_quote = declared_value_plus_nickel.quote(kCartItem);

    EXPECT_NE(base_quote, flat_quote);
    EXPECT_NE(base_quote, declared_value_quote);
    EXPECT_NE(flat_quote, declared_value_quote);
    EXPECT_EQ(base_quote, 1000);
    EXPECT_EQ(flat_quote, 1155);
    EXPECT_EQ(declared_value_quote, 1125);
}

TEST(ShippingCalculatorTest, RoundUpToNickelLeavesAnAlreadyRoundAmountUnchanged) {
    const RoundUpToNickelPolicy rounding;

    EXPECT_EQ(rounding.round(1150), 1150);
    EXPECT_EQ(rounding.round(1151), 1155);
}

}  // namespace
}  // namespace crtp21
