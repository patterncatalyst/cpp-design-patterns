#include "carrier_profile_serialization.hpp"

#include <gtest/gtest.h>

namespace exprtmpl {
namespace {

TEST(CarrierProfileSerializationTest, RoundTripReproducesAnEqualProfile) {
    const CarrierProfile original{"Northline Freight", 1.35, 18000.0};

    const std::string serialized = serialize(original);
    const CarrierProfile round_tripped = deserialize(serialized);

    EXPECT_EQ(original, round_tripped);
}

TEST(CarrierProfileSerializationTest, SerializedTextCarriesOneLinePerDescribedField) {
    const CarrierProfile profile{"Regional Express", 1.8, 9000.0};

    const std::string serialized = serialize(profile);

    EXPECT_NE(serialized.find("carrier_name=Regional Express"), std::string::npos);
    EXPECT_NE(serialized.find("base_rate_multiplier="), std::string::npos);
    EXPECT_NE(serialized.find("max_weight_kg="), std::string::npos);
}

}  // namespace
}  // namespace exprtmpl
