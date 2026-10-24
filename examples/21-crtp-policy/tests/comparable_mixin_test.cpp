#include "comparable_mixin.hpp"

#include <gtest/gtest.h>

#include "quote_types.hpp"

namespace crtp21 {
namespace {

TEST(ComparableMixinTest, ShipmentQuoteOrdersByTotalCents) {
    const ShipmentQuote cheaper(999);
    const ShipmentQuote pricier(1025);

    EXPECT_TRUE(cheaper < pricier);
    EXPECT_TRUE(pricier > cheaper);
    EXPECT_FALSE(cheaper == pricier);
    EXPECT_TRUE(ShipmentQuote(999) == cheaper);
}

TEST(ComparableMixinTest, CarrierRatingOrdersByScoreEvenThoughItSharesNoBaseWithShipmentQuote) {
    const CarrierRating higher_rated(4.6);
    const CarrierRating lower_rated(4.2);

    EXPECT_TRUE(higher_rated > lower_rated);
    EXPECT_TRUE(lower_rated < higher_rated);
    EXPECT_FALSE(higher_rated == lower_rated);
}

TEST(ComparableMixinTest, TheSameMixinWorksAcrossTwoUnrelatedDerivedTypes) {
    // ShipmentQuote orders by an integer Cents key; CarrierRating orders by
    // a floating-point score. Neither inherits from the other, and neither
    // knows the other exists. Comparable<Derived> supplies operator<,
    // operator==, and operator> to both from the exact same template.
    EXPECT_TRUE(ShipmentQuote(500) < ShipmentQuote(600));
    EXPECT_TRUE(CarrierRating(3.0) < CarrierRating(3.5));
}

}  // namespace
}  // namespace crtp21
