#include <memory>

#include <gtest/gtest.h>

#include "classic_visitor.hpp"
#include "shipping_domain.hpp"

namespace {

using rules::AndNode;
using rules::DeclaredValueAboveNode;
using rules::NotNode;
using rules::OrNode;
using rules::WeightAboveNode;
using rules::ZoneIsNode;
using shipping::DestinationZone;
using shipping::Shipment;

TEST(ClassicVisitorTest, WeightAbovePredicateMatchesOverThreshold) {
    const WeightAboveNode node(10.0);
    const Shipment heavy{12.0, DestinationZone::Local, 0};
    const Shipment light{5.0, DestinationZone::Local, 0};

    EXPECT_TRUE(rules::evaluate_classic(node, heavy));
    EXPECT_FALSE(rules::evaluate_classic(node, light));
}

TEST(ClassicVisitorTest, ZoneIsPredicateMatchesExactZone) {
    const ZoneIsNode node(DestinationZone::National);
    const Shipment matching{1.0, DestinationZone::National, 0};
    const Shipment other{1.0, DestinationZone::Regional, 0};

    EXPECT_TRUE(rules::evaluate_classic(node, matching));
    EXPECT_FALSE(rules::evaluate_classic(node, other));
}

TEST(ClassicVisitorTest, DeclaredValueAbovePredicateMatchesOverThreshold) {
    const DeclaredValueAboveNode node(50000);
    const Shipment valuable{1.0, DestinationZone::Local, 80000};
    const Shipment modest{1.0, DestinationZone::Local, 10000};

    EXPECT_TRUE(rules::evaluate_classic(node, valuable));
    EXPECT_FALSE(rules::evaluate_classic(node, modest));
}

TEST(ClassicVisitorTest, AndNodeRequiresBothSides) {
    AndNode node(std::make_unique<WeightAboveNode>(10.0), std::make_unique<ZoneIsNode>(DestinationZone::National));
    const Shipment both_true{12.0, DestinationZone::National, 0};
    const Shipment one_false{5.0, DestinationZone::National, 0};

    EXPECT_TRUE(rules::evaluate_classic(node, both_true));
    EXPECT_FALSE(rules::evaluate_classic(node, one_false));
}

TEST(ClassicVisitorTest, OrNodeRequiresEitherSide) {
    OrNode node(std::make_unique<WeightAboveNode>(10.0), std::make_unique<ZoneIsNode>(DestinationZone::National));
    const Shipment both_false{5.0, DestinationZone::Regional, 0};
    const Shipment one_true{5.0, DestinationZone::National, 0};

    EXPECT_FALSE(rules::evaluate_classic(node, both_false));
    EXPECT_TRUE(rules::evaluate_classic(node, one_true));
}

TEST(ClassicVisitorTest, NotNodeInvertsItsInner) {
    NotNode node(std::make_unique<ZoneIsNode>(DestinationZone::National));
    const Shipment matching{1.0, DestinationZone::National, 0};
    const Shipment other{1.0, DestinationZone::Regional, 0};

    EXPECT_FALSE(rules::evaluate_classic(node, matching));
    EXPECT_TRUE(rules::evaluate_classic(node, other));
}

TEST(ClassicVisitorTest, DescribeRendersNestedRuleAsReadableString) {
    AndNode node(std::make_unique<WeightAboveNode>(10.0), std::make_unique<ZoneIsNode>(DestinationZone::National));

    EXPECT_EQ(rules::describe_classic(node), "(weight above 10.0kg AND zone is National)");
}

TEST(ClassicVisitorTest, DescribeRendersNotAndDeclaredValue) {
    NotNode node(std::make_unique<DeclaredValueAboveNode>(50000));

    EXPECT_EQ(rules::describe_classic(node), "NOT declared value above $500.00");
}

}  // namespace
