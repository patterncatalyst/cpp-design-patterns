#include "rate_model_deducing_this.hpp"

#include <gtest/gtest.h>

#include "rate_model.hpp"

#if __cplusplus >= 202302L

namespace crtp21 {
namespace {

TEST(DeducingThisRateModelTest, QuoteDispatchesToDescribeWithNoCrtpTemplateArgument) {
    const DeducedFlatRateModel model(999);

    EXPECT_EQ(model.quote(Shipment{/*weight_kg=*/0.5, /*declared_value_cents=*/0}), 999);
}

TEST(DeducingThisRateModelTest, CrtpAndDeducingThisAgreeOnTheSameShipment) {
    const FlatRateModel crtp_model(999);
    const DeducedFlatRateModel deduced_model(999);
    const Shipment shipment{/*weight_kg=*/3.5, /*declared_value_cents=*/0};

    EXPECT_EQ(crtp_model.quote(shipment), deduced_model.quote(shipment));
}

}  // namespace
}  // namespace crtp21

#endif  // __cplusplus >= 202302L
