#include "carrier_contract.hpp"

#include <gtest/gtest.h>

#include <utility>

#include "shipping_route.hpp"

namespace warehouse {
namespace {

TEST(CarrierContractTest, CopyIsADeepCopyIndependentOfTheOriginal) {
    const Cents tiers[] = {100, 200, 300};
    CarrierContract original("northwind", tiers, 3);

    CarrierContract copy = original;
    copy.set_tier_rate(1, 999);

    EXPECT_EQ(original.tier_rate(1), 200);
    EXPECT_EQ(copy.tier_rate(1), 999);
}

TEST(CarrierContractTest, CopyAssignmentIsAlsoADeepCopy) {
    const Cents tiers_a[] = {100, 200};
    const Cents tiers_b[] = {10, 20, 30};
    CarrierContract a("northwind", tiers_a, 2);
    CarrierContract b("cardinal", tiers_b, 3);

    b = a;
    b.set_tier_rate(0, 555);

    EXPECT_EQ(a.tier_rate(0), 100);
    EXPECT_EQ(b.tier_rate(0), 555);
    EXPECT_EQ(b.tier_count(), 2u);
}

TEST(CarrierContractTest, MoveTransfersTheTableAndLeavesTheSourceEmpty) {
    const Cents tiers[] = {100, 200, 300};
    CarrierContract original("northwind", tiers, 3);

    CarrierContract moved_to = std::move(original);

    EXPECT_EQ(moved_to.tier_rate(2), 300);
    EXPECT_EQ(moved_to.carrier_name(), "northwind");
    EXPECT_EQ(original.tier_count(), 0u);
}

TEST(CarrierContractTest, MoveAssignmentReleasesTheDestinationsOwnTableFirst) {
    const Cents tiers_a[] = {100, 200};
    const Cents tiers_b[] = {10, 20, 30};
    CarrierContract a("northwind", tiers_a, 2);
    CarrierContract b("cardinal", tiers_b, 3);

    b = std::move(a);

    EXPECT_EQ(b.tier_rate(1), 200);
    EXPECT_EQ(b.carrier_name(), "northwind");
    EXPECT_EQ(a.tier_count(), 0u);
}

TEST(CarrierContractSharingTest, RoutesSharingAContractObserveTheSameRenegotiatedRate) {
    const Cents tiers[] = {250, 400, 650};
    auto contract = std::make_shared<CarrierContract>("northwind", tiers, 3);
    ShippingRoute north("north-corridor", contract, /*tier=*/1);
    ShippingRoute south("south-corridor", contract, /*tier=*/1);

    EXPECT_EQ(north.quote(), 400);
    EXPECT_EQ(south.quote(), 400);

    contract->set_tier_rate(1, 475);

    EXPECT_EQ(north.quote(), 475);
    EXPECT_EQ(south.quote(), 475);
}

TEST(CarrierContractSharingTest, SharedPtrCopyReferencesTheSameUnderlyingContract) {
    const Cents tiers[] = {250, 400, 650};
    auto contract = std::make_shared<CarrierContract>("northwind", tiers, 3);
    std::shared_ptr<CarrierContract> copy_of_pointer = contract;

    EXPECT_EQ(contract.use_count(), 2);
    copy_of_pointer->set_tier_rate(0, 111);

    EXPECT_EQ(contract->tier_rate(0), 111);
}

}  // namespace
}  // namespace warehouse
