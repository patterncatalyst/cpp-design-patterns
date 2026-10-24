#include "route_metrics.hpp"

#include <gtest/gtest.h>

#include "construction_tracking.hpp"
#include "eager_route_metrics.hpp"

namespace exprtmpl {
namespace {

TEST(ExpressionTemplateTest, WeightedSumMatchesEagerComputation) {
    const RouteMetrics leg_one(120.0, 18.5, 0.75);
    const RouteMetrics leg_two(64.0, 9.0, 1.25);
    const RouteMetrics leg_three(210.0, 31.0, 0.5);
    const RouteMetrics lazy_total = 0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three;

    const NaiveRouteMetrics naive_one(120.0, 18.5, 0.75);
    const NaiveRouteMetrics naive_two(64.0, 9.0, 1.25);
    const NaiveRouteMetrics naive_three(210.0, 31.0, 0.5);
    const NaiveRouteMetrics eager_total = 0.5 * naive_one + 0.3 * naive_two + 0.2 * naive_three;

    EXPECT_DOUBLE_EQ(lazy_total.distance_km(), eager_total.distance_km());
    EXPECT_DOUBLE_EQ(lazy_total.fuel_liters(), eager_total.fuel_liters());
    EXPECT_DOUBLE_EQ(lazy_total.dwell_hours(), eager_total.dwell_hours());
}

TEST(ExpressionTemplateTest, LazyExpressionConstructsExactlyOneResult) {
    const RouteMetrics leg_one(120.0, 18.5, 0.75);
    const RouteMetrics leg_two(64.0, 9.0, 1.25);
    const RouteMetrics leg_three(210.0, 31.0, 0.5);

    reset_construction_count();
    const RouteMetrics lazy_total = 0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three;

    // Two '+' and three scalar '*' nest five expression-template node
    // objects (three RouteScaled, two RouteSum), none of them a
    // RouteMetrics; only the final assignment above runs RouteMetrics's
    // evaluating constructor, exactly once.
    EXPECT_EQ(construction_count(), 1u);
    EXPECT_GT(lazy_total.distance_km(), 0.0);
}

TEST(ExpressionTemplateTest, EagerEquivalentConstructsOneIntermediatePerOperator) {
    const NaiveRouteMetrics naive_one(120.0, 18.5, 0.75);
    const NaiveRouteMetrics naive_two(64.0, 9.0, 1.25);
    const NaiveRouteMetrics naive_three(210.0, 31.0, 0.5);

    reset_construction_count();
    const NaiveRouteMetrics eager_total = 0.5 * naive_one + 0.3 * naive_two + 0.2 * naive_three;

    // Three scalar multiplies and two adds, each returning a brand new
    // NaiveRouteMetrics by value: five intermediate objects for a
    // three-term weighted sum, five times what the lazy expression above
    // needed for the identical result.
    EXPECT_EQ(construction_count(), 5u);
    EXPECT_GT(eager_total.distance_km(), 0.0);
}

}  // namespace
}  // namespace exprtmpl
