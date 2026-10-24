#include "shipping_facade.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "legacy_rate_api.hpp"
#include "rate_adapter.hpp"

namespace shipping {
namespace {

ShippingFacade make_facade() {
    return ShippingFacade(
        std::make_unique<LegacyFreightRateAdapter>(legacy_freight::LegacyGlobalFreightRateApi(/*surcharge_cents=*/50)),
        LabelRenderer("GlobalFreight"), TrackingRegistry("track.globalfreight.example"));
}

TEST(ShippingFacadeTest, QuoteAndLabelCombinesAllThreeSubsystemsInOneCall) {
    ShippingFacade facade = make_facade();
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    const ShippingQuote quote = facade.quote_and_label(shipment, /*sequence=*/1);

    EXPECT_EQ(quote.price_cents, 600);
    EXPECT_NE(quote.label.find("GlobalFreight"), std::string::npos);
    EXPECT_NE(quote.label.find(quote.tracking_number), std::string::npos);
    EXPECT_NE(quote.tracking_url.find(quote.tracking_number), std::string::npos);
    EXPECT_NE(quote.tracking_url.find("track.globalfreight.example"), std::string::npos);
}

TEST(ShippingFacadeTest, RejectsAMalformedShipmentWithoutTheClientOrchestratingAnything) {
    ShippingFacade facade = make_facade();
    const Shipment malformed{-1.0, DestinationZone::Local, 0};

    EXPECT_THROW(facade.quote_and_label(malformed, 1), std::invalid_argument);
}

TEST(TrackingRegistryTest, BuildsASequentialTrackingNumberAndMatchingUrl) {
    TrackingRegistry registry("tracking.example");
    const std::string number = registry.issue_tracking_number(7);

    EXPECT_EQ(number, "TRK-1007");
    EXPECT_EQ(registry.tracking_url(number), "https://tracking.example/track/TRK-1007");
}

#if __cplusplus >= 202302L
TEST(ShippingFacadeTest, TryQuoteAndLabelReturnsATypedErrorForAMalformedShipment) {
    ShippingFacade facade = make_facade();
    const Shipment malformed{-1.0, DestinationZone::Local, 0};

    const auto result = facade.try_quote_and_label(malformed, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "shipment weight_kg cannot be negative");
}

TEST(ShippingFacadeTest, TryQuoteAndLabelReturnsTheQuoteForAValidShipment) {
    ShippingFacade facade = make_facade();
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    const auto result = facade.try_quote_and_label(shipment, 1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->price_cents, 600);
}
#endif

}  // namespace
}  // namespace shipping
