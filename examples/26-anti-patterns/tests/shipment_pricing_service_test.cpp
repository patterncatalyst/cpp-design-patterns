#include <gtest/gtest.h>

#include <stdexcept>

#include "shipment_pricing_service.hpp"

namespace antipattern26 {
namespace {

// Proof #2, half two: the simplified design runs two independently
// configured instances in the same process without either one affecting
// the other. There is no shared state to leak, because RateSettings is a
// value each ShipmentPricingService owns, not a global slot every
// calculator reaches into.
TEST(ShipmentPricingServiceTest, TwoIndependentlyConfiguredInstancesNeverAffectEachOther) {
    const ShipmentPricingService domestic(RateSettings{1.0, 2.50});
    const ShipmentPricingService international(RateSettings{1.35, 9.00});

    const PricingResult domestic_result = domestic.price(100.0);
    const PricingResult international_result = international.price(100.0);

    EXPECT_DOUBLE_EQ(domestic_result.total_cost, 102.50);
    EXPECT_FALSE(domestic_result.surcharge_applied);

    EXPECT_DOUBLE_EQ(international_result.total_cost, 144.0);
    EXPECT_TRUE(international_result.surcharge_applied);

    // Re-checking domestic after international ran is the direct contrast
    // with ShippingRateRegistrySingletonTest above: computing one
    // configuration here never moves the other.
    EXPECT_DOUBLE_EQ(domestic.price(100.0).total_cost, 102.50);
}

// "Substituting a fake" for a value-shaped dependency is simply
// constructing a different value. No interface, no mock framework, and
// nothing to register or reset once the test ends.
TEST(ShipmentPricingServiceTest, TestSubstitutesAFakeRateSettingsWithNoSharedStateToReset) {
    const RateSettings fake_promotional_rate{0.80, 0.0};
    const ShipmentPricingService promo(fake_promotional_rate);

    EXPECT_DOUBLE_EQ(promo.price(100.0).total_cost, 80.0);
    // surcharge_applied flags any deviation from the 1.0 default, discount
    // included; the field answers "is this not the baseline rate," not
    // "is this specifically higher than baseline."
    EXPECT_TRUE(promo.price(100.0).surcharge_applied);
}

TEST(ShipmentPricingServiceTest, PriceRejectsANonPositiveBaseCost) {
    const ShipmentPricingService service(RateSettings{1.0, 0.0});
    EXPECT_THROW(service.price(0.0), std::invalid_argument);
    EXPECT_THROW(service.price(-1.0), std::invalid_argument);
}

}  // namespace
}  // namespace antipattern26
