#include "rate_adapter.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace shipping {
namespace {

TEST(ZoneToLegacyCodeTest, MapsEachZoneToItsLegacyCode) {
    EXPECT_EQ(zone_to_legacy_code(DestinationZone::Local), 0);
    EXPECT_EQ(zone_to_legacy_code(DestinationZone::Regional), 1);
    EXPECT_EQ(zone_to_legacy_code(DestinationZone::National), 2);
    EXPECT_EQ(zone_to_legacy_code(DestinationZone::International), 3);
}

TEST(LegacyFreightRateAdapterTest, TranslatesShipmentIntoLegacyUnitsAndBack) {
    LegacyFreightRateAdapter adapter(legacy_freight::LegacyGlobalFreightRateApi(/*surcharge_cents=*/50));
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    // 3.5kg -> 3500g -> 350 cents; Regional -> +200; + surcharge 50 = 600.
    EXPECT_EQ(adapter.quote(shipment), 600);
}

TEST(LegacyFreightRateAdapterTest, SatisfiesTheRateSourceInterface) {
    std::unique_ptr<RateSource> source =
        std::make_unique<LegacyFreightRateAdapter>(legacy_freight::LegacyGlobalFreightRateApi(/*surcharge_cents=*/0));
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    EXPECT_EQ(source->quote(shipment), 100);
}

}  // namespace
}  // namespace shipping
