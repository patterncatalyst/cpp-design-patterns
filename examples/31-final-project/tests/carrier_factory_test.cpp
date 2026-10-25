#include "carrier_factory.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

#include "shipment.hpp"

namespace finalproject31 {
namespace {

TEST(CarrierFactoryTest, BuildsAFlatRateCarrierForGroundline) {
    CarrierFactory factory;

    CarrierProfile profile = factory.create(CarrierRequest{"GROUNDLINE", "LOCAL"});

    EXPECT_EQ(profile.carrier_name, "Groundline Parcel");
    EXPECT_EQ(profile.sla_hours, 72);
    ASSERT_NE(profile.rate_strategy, nullptr);

    Shipment shipment("SHIP-1", profile.carrier_name, "LOCAL", 30.0, 0);
    // Groundline is flat-rate: a 30kg shipment quotes the same as a 1kg one.
    EXPECT_EQ(profile.rate_strategy->quote(shipment), 899);
}

TEST(CarrierFactoryTest, BuildsAWeightTieredCarrierForSkyhaul) {
    CarrierFactory factory;

    CarrierProfile profile = factory.create(CarrierRequest{"SKYHAUL", "NATIONAL"});

    EXPECT_EQ(profile.carrier_name, "Skyhaul Express");
    EXPECT_EQ(profile.sla_hours, 24);
    ASSERT_NE(profile.rate_strategy, nullptr);

    Shipment shipment("SHIP-1", profile.carrier_name, "NATIONAL", 8.0, 0);
    // 8kg falls in Skyhaul's second tier: 1600 + round(140 * 8) = 2720.
    EXPECT_EQ(profile.rate_strategy->quote(shipment), 2720);
}

TEST(CarrierFactoryTest, CreateThrowsForAnUnrecognizedCarrierCode) {
    CarrierFactory factory;

    EXPECT_THROW(factory.create(CarrierRequest{"PIGEON-POST", "LOCAL"}), std::invalid_argument);
}

#if __cplusplus >= 202302L
TEST(CarrierFactoryTest, TryCreateReturnsAnExpectedValueForAKnownCarrier) {
    CarrierFactory factory;

    auto result = factory.try_create(CarrierRequest{"GROUNDLINE", "LOCAL"});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->carrier_name, "Groundline Parcel");
}

TEST(CarrierFactoryTest, TryCreateReturnsUnexpectedForAnUnrecognizedCarrierCode) {
    CarrierFactory factory;

    auto result = factory.try_create(CarrierRequest{"PIGEON-POST", "LOCAL"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CarrierFactoryError::UnknownCarrierCode);
}
#endif

}  // namespace
}  // namespace finalproject31
