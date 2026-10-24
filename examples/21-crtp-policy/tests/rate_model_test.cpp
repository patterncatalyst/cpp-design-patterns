#include "rate_model.hpp"

#include <gtest/gtest.h>

namespace crtp21 {
namespace {

// A function templated on Derived, taking a RateModel<Derived> by const
// reference, proves CRTP dispatch works the same way through a base-class
// reference as it does on the concrete type directly: RateModel<Derived>
// is a different, fully resolved type for every Derived, so there is
// still no vtable lookup here, only a template instantiated once per
// Derived the compiler actually sees used.
template <typename Derived>
Cents quote_via_base(const RateModel<Derived>& model, const Shipment& shipment) {
    return model.quote(shipment);
}

TEST(RateModelTest, FlatRateModelQuotesTheFlatRateRegardlessOfShipment) {
    const FlatRateModel model(999);

    EXPECT_EQ(model.quote(Shipment{/*weight_kg=*/0.5, /*declared_value_cents=*/0}), 999);
    EXPECT_EQ(model.quote(Shipment{/*weight_kg=*/40.0, /*declared_value_cents=*/500000}), 999);
}

TEST(RateModelTest, WeightedRateModelAddsTheRoundedUpVariableCost) {
    const WeightedRateModel model(/*base_cents=*/500, /*per_kg_cents=*/150);

    EXPECT_EQ(model.quote(Shipment{/*weight_kg=*/3.5, /*declared_value_cents=*/0}), 1025);
    // 1.01 kg * 150 cents/kg = 151.5 cents, a value that lands between whole
    // cents, exercising the manual round-up to 152.
    EXPECT_EQ(model.quote(Shipment{/*weight_kg=*/1.01, /*declared_value_cents=*/0}), 500 + 152);
}

TEST(RateModelTest, EachDerivedTypeDispatchesToItsOwnComputeThroughTheBaseReference) {
    const FlatRateModel flat_model(999);
    const WeightedRateModel weighted_model(/*base_cents=*/500, /*per_kg_cents=*/150);
    const Shipment shipment{/*weight_kg=*/3.5, /*declared_value_cents=*/0};

    EXPECT_EQ(quote_via_base(flat_model, shipment), 999);
    EXPECT_EQ(quote_via_base(weighted_model, shipment), 1025);
    EXPECT_NE(quote_via_base(flat_model, shipment), quote_via_base(weighted_model, shipment));
}

}  // namespace
}  // namespace crtp21
