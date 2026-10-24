#include "any_rate_quoter.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "rate_quoters.hpp"
#include "shipping_domain.hpp"

namespace shipping {
namespace {

TEST(AnyRateQuoterTest, HoldsAndDispatchesToDistinctConcreteTypes) {
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    std::vector<AnyRateQuoter> quoters;
    quoters.emplace_back(FlatRateQuoter{999});
    quoters.emplace_back(WeightBasedQuoter{500, 150});
    quoters.emplace_back(InsuredQuoter(AnyRateQuoter(WeightBasedQuoter{500, 150}), /*cents_per_declared_dollar=*/1.5));

    ASSERT_EQ(quoters.size(), 3u);
    EXPECT_EQ(quoters[0].quote(shipment), 999);
    EXPECT_EQ(quoters[1].quote(shipment), 1025);
    EXPECT_EQ(quoters[2].quote(shipment), 1205);  // 1025 base + 120.00 declared dollars * 1.5 cents
}

TEST(AnyRateQuoterTest, CopyIsIndependentOfTheOriginalsMutableState) {
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    AnyRateQuoter original(SurgeQuoter{1000, 25});
    EXPECT_EQ(original.quote(shipment), 1000);  // call 1: no surge yet

    AnyRateQuoter copy = original;  // deep clone: copy starts from one prior call
    EXPECT_EQ(copy.quote(shipment), 1025);
    EXPECT_EQ(copy.quote(shipment), 1050);
    EXPECT_EQ(copy.quote(shipment), 1075);  // copy's own counter has advanced through 4 calls total

    // The original's own surge counter must be untouched by the copy's
    // extra calls: its second call is still its second call, not its fifth.
    EXPECT_EQ(original.quote(shipment), 1025);
}

TEST(AnyRateQuoterTest, CopyAssignmentReplacesTheTargetWithAnIndependentClone) {
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    AnyRateQuoter quoter_a(FlatRateQuoter{500});
    AnyRateQuoter quoter_b(SurgeQuoter{1000, 25});
    quoter_b.quote(shipment);  // quoter_b's internal counter now at 1

    quoter_a = quoter_b;  // quoter_a becomes an independent clone of quoter_b's state

    EXPECT_EQ(quoter_a.quote(shipment), 1025);   // inherits quoter_b's call count, then advances its own
    EXPECT_EQ(quoter_b.quote(shipment), 1025);   // quoter_b's own counter, untouched by quoter_a, is separately at 2
}

}  // namespace
}  // namespace shipping
