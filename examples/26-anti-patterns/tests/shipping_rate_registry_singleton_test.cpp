#include <gtest/gtest.h>

#include "rate_calculator_factory.hpp"
#include "shipping_rate_registry.hpp"

namespace antipattern26 {
namespace {

TEST(ShippingRateRegistrySingletonTest, InstanceReturnsTheSameObjectFromEveryCallSite) {
    ShippingRateRegistry& first = ShippingRateRegistry::instance();
    ShippingRateRegistry& second = ShippingRateRegistry::instance();
    EXPECT_EQ(&first, &second);
}

// Proof #2, half one: global mutable state produces spooky action at a
// distance. Region A obtains a calculator while the registry holds its
// rate. Region B then configures its own rate, touching nothing that
// belongs to region A. Region A's calculator still changes, because both
// calculators read the same mutable global at calculate() time rather than
// capturing their own setting at construction time. This is the structural
// reason the over-patterned design cannot run two independently configured
// regions in the same process: there is only one slot to write into.
TEST(ShippingRateRegistrySingletonTest,
     ReconfiguringTheRegistryRetroactivelyChangesAnAlreadyObtainedCalculator) {
    ShippingRateRegistry::instance().set_surcharge_multiplier(1.10);
    auto region_a_calculator = RateCalculatorFactoryProvider::instance().factory().create_calculator();
    EXPECT_DOUBLE_EQ(region_a_calculator->calculate(100.0), 110.0);

    // A second module configures its own region. There is only one slot
    // to write to: this call overwrites region A's setting too.
    ShippingRateRegistry::instance().set_surcharge_multiplier(1.30);
    auto region_b_calculator = RateCalculatorFactoryProvider::instance().factory().create_calculator();
    EXPECT_DOUBLE_EQ(region_b_calculator->calculate(100.0), 130.0);

    // The bug: region_a_calculator was never touched directly, yet it now
    // reports region B's rate, because both read the same mutable global.
    EXPECT_DOUBLE_EQ(region_a_calculator->calculate(100.0), 130.0);
}

}  // namespace
}  // namespace antipattern26
