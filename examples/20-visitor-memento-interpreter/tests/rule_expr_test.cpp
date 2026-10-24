#include <memory>

#include <gtest/gtest.h>

#include "classic_visitor.hpp"
#include "rule_expr.hpp"
#include "shipping_domain.hpp"

namespace {

using shipping::DestinationZone;
using shipping::Shipment;

TEST(RuleExprTest, VariantVisitorAgreesWithClassicVisitorOnEquivalentTree) {
    const Shipment shipment{12.0, DestinationZone::National, 30000};

    rules::AndNode classic_tree(std::make_unique<rules::WeightAboveNode>(10.0),
                                 std::make_unique<rules::ZoneIsNode>(DestinationZone::National));
    rules::RuleExpr variant_tree =
        rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National));

    EXPECT_EQ(rules::evaluate_classic(classic_tree, shipment), rules::evaluate(variant_tree, shipment));
    EXPECT_EQ(rules::describe_classic(classic_tree), rules::describe(variant_tree));
}

TEST(RuleExprTest, VariantVisitorAgreesWithClassicVisitorWhenPredicateFails) {
    const Shipment shipment{5.0, DestinationZone::Regional, 30000};

    rules::AndNode classic_tree(std::make_unique<rules::WeightAboveNode>(10.0),
                                 std::make_unique<rules::ZoneIsNode>(DestinationZone::National));
    rules::RuleExpr variant_tree =
        rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National));

    EXPECT_EQ(rules::evaluate_classic(classic_tree, shipment), rules::evaluate(variant_tree, shipment));
    EXPECT_FALSE(rules::evaluate(variant_tree, shipment));
}

TEST(RuleExprTest, NotExprInvertsItsInner) {
    rules::RuleExpr expr = rules::rule_not(rules::zone_is(DestinationZone::National));
    const Shipment matching{1.0, DestinationZone::National, 0};
    const Shipment other{1.0, DestinationZone::Regional, 0};

    EXPECT_FALSE(rules::evaluate(expr, matching));
    EXPECT_TRUE(rules::evaluate(expr, other));
}

TEST(RuleExprTest, NestingOrderChangesTheEvaluatedResult) {
    const Shipment shipment{5.0, DestinationZone::National, 80000};

    rules::RuleExpr outer_or =
        rules::rule_or(rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National)),
                        rules::declared_value_above(50000));
    rules::RuleExpr outer_and = rules::rule_and(
        rules::weight_above(10.0),
        rules::rule_or(rules::zone_is(DestinationZone::National), rules::declared_value_above(50000)));

    EXPECT_TRUE(rules::evaluate(outer_or, shipment));
    EXPECT_FALSE(rules::evaluate(outer_and, shipment));
}

TEST(RuleExprTest, DeeplyNestedExpressionEvaluatesCorrectly) {
    const Shipment shipment{20.0, DestinationZone::International, 90000};

    rules::RuleExpr expr = rules::rule_and(
        rules::rule_or(rules::weight_above(50.0), rules::zone_is(DestinationZone::International)),
        rules::rule_not(rules::declared_value_above(100000)));

    EXPECT_TRUE(rules::evaluate(expr, shipment));
}

TEST(RuleExprTest, CloneProducesAnIndependentTreeWithTheSameResult) {
    rules::RuleExpr original = rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National));
    rules::RuleExpr cloned = rules::clone(original);

    const Shipment shipment{12.0, DestinationZone::National, 0};

    EXPECT_EQ(rules::evaluate(original, shipment), rules::evaluate(cloned, shipment));
    EXPECT_EQ(rules::describe(original), rules::describe(cloned));
}

}  // namespace
