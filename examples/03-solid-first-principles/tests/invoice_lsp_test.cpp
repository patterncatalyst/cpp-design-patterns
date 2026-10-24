#include "invoice_rules.hpp"

#include <gtest/gtest.h>

#include <array>
#include <functional>

namespace billing {
namespace {

// The whole point of this file: a function written once, against the
// InvoiceRule base class, that behaves identically no matter which
// concrete rule it is handed. If LoyaltyDiscountRule broke its parent's
// contract, one of the calls below would behave differently from the
// other for no reason visible at this call site.
Cents adjustment_through_base_class(const InvoiceRule& rule, Cents subtotal) {
    return rule.adjustment_for(subtotal);
}

TEST(LiskovSubstitutionTest, BothDiscountRulesAreUsableThroughTheSameBaseClassReference) {
    const PercentageDiscountRule standard(0.10);
    const LoyaltyDiscountRule loyalty(/*base_rate=*/0.03, /*loyalty_years=*/4);

    EXPECT_EQ(adjustment_through_base_class(standard, 254500), -25450);
    EXPECT_EQ(adjustment_through_base_class(loyalty, 254500), -17815);
}

TEST(LiskovSubstitutionTest, LoyaltyDiscountAddsOnePercentPerYearUpToTenYears) {
    const LoyaltyDiscountRule two_year_customer(/*base_rate=*/0.05, /*loyalty_years=*/2);

    // 5% base + 2% loyalty bonus = 7% of a $1,000.00 subtotal.
    EXPECT_EQ(two_year_customer.adjustment_for(100000), -7000);
}

TEST(LiskovSubstitutionTest, LoyaltyDiscountNeverExceedsTheSubtotalEvenAtTheYearsCap) {
    // 50 years requested, capped at 10 inside LoyaltyDiscountRule, so the
    // effective bonus is 10%, not 50%; the combined 105% rate is then
    // re-clamped to 100% by the parent's adjustment_for regardless, the
    // same invariant PercentageDiscountRule enforces for every subtype.
    const LoyaltyDiscountRule long_time_customer(/*base_rate=*/0.95, /*loyalty_years=*/50);

    const Cents adjustment = long_time_customer.adjustment_for(100000);

    EXPECT_GE(adjustment, -100000);
    EXPECT_EQ(adjustment, -100000);
}

TEST(LiskovSubstitutionTest, DiscountRuleContractHoldsAcrossBothConcreteTypes) {
    // Held by reference, not by value: a PercentageDiscountRule array of
    // values would slice LoyaltyDiscountRule down to its parent and lose
    // the override under test. References (or pointers) are what let a
    // base-typed container actually hold heterogeneous subtypes.
    const PercentageDiscountRule standard_discount(0.25);
    const LoyaltyDiscountRule loyalty_discount(/*base_rate=*/0.2, /*loyalty_years=*/5);
    const std::array<std::reference_wrapper<const PercentageDiscountRule>, 2> rules_under_test{
        std::cref(standard_discount), std::cref(loyalty_discount)};
    const Cents subtotal = 80000;

    for (const PercentageDiscountRule& rule : rules_under_test) {
        const Cents adjustment = adjustment_through_base_class(rule, subtotal);
        EXPECT_LE(adjustment, 0) << rule.name();
        EXPECT_GE(adjustment, -subtotal) << rule.name();
    }
}

}  // namespace
}  // namespace billing
