#include "legacy_carrier_zone_lookup.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace breakdep28 {
namespace {

using ::testing::Return;

class MockZoneLookupPort : public ZoneLookupPort {
public:
    MOCK_METHOD(int, zone_for, (const std::string& postal_code), (const, override));
};

TEST(ShipmentRoutingAdvisorTest, DelegatorForwardsToTheStaticLegacyMethod) {
    CarrierZoneLookupDelegator delegator;

    EXPECT_EQ(delegator.zone_for("02108"), LegacyCarrierZoneLookup::zone_for("02108"));
}

TEST(ShipmentRoutingAdvisorTest, RoutesThroughAnInjectedFakeWithNoLegacyTableInvolved) {
    MockZoneLookupPort lookup;
    EXPECT_CALL(lookup, zone_for("99501")).WillOnce(Return(3));

    ShipmentRoutingAdvisor advisor(lookup);
    const auto hint = advisor.route_hint("99501");

    EXPECT_NE(hint.find("long-haul"), std::string::npos);
}

TEST(ShipmentRoutingAdvisorTest, ExistingLegacyCallSiteStillWorksUnchanged) {
    // ZoneSurchargeReport never heard about ZoneLookupPort or the
    // delegator; Introduce Instance Delegator exists precisely so this
    // call site needs no change at all.
    const auto description = ZoneSurchargeReport::describe("02108");

    EXPECT_NE(description.find("02108"), std::string::npos);
}

}  // namespace
}  // namespace breakdep28
