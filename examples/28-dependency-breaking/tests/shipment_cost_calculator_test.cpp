#include "legacy_fuel_surcharge.hpp"

#include <gtest/gtest.h>

namespace breakdep28 {
namespace {

class FixedFuelSurchargeSource : public FuelSurchargeSource {
public:
    explicit FixedFuelSurchargeSource(double multiplier) : multiplier_(multiplier) {}
    double multiplier() const override { return multiplier_; }

private:
    double multiplier_;
};

TEST(ShipmentCostCalculatorTest, LiveSourceReadsTheLegacyGlobal) {
    legacy_global::set_fuel_surcharge(1.20);
    LiveFuelSurchargeSource live_source;
    ShipmentCostCalculator calculator(live_source);

    EXPECT_DOUBLE_EQ(calculator.total_cost(100.0), 120.0);
}

TEST(ShipmentCostCalculatorTest, FixedSourceIgnoresTheLegacyGlobalEntirely) {
    legacy_global::set_fuel_surcharge(1.75);  // deliberately far from the fixed rate below
    FixedFuelSurchargeSource fixed_source(1.10);
    ShipmentCostCalculator calculator(fixed_source);

    EXPECT_DOUBLE_EQ(calculator.total_cost(100.0), 110.0);

    // Changing the legacy global after construction still changes nothing:
    // this calculator was never wired to it in the first place.
    legacy_global::set_fuel_surcharge(3.0);
    EXPECT_DOUBLE_EQ(calculator.total_cost(100.0), 110.0);
}

}  // namespace
}  // namespace breakdep28
