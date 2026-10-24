#include "static_dispatch_variant.hpp"

#include <gtest/gtest.h>

namespace orderexport {
namespace {

Order sample_order() {
    return Order{"ORD-77", "Riverside Hardware Co.", {OrderLine{"SKU-100", 3}, OrderLine{"SKU-205", 1}}};
}

TEST(CrtpSummarySkeletonTest, SummarizeDispatchesStaticallyToDescribe) {
    CrtpOrderSummary summary;

    EXPECT_EQ(summary.summarize(sample_order()), "ORD-77: 2 line(s)");
}

#if __cplusplus >= 202302L
TEST(DeducingThisSummarySkeletonTest, SummarizeDeducesTheMostDerivedTypeWithNoCrtpParameter) {
    DeducedOrderSummary summary;

    EXPECT_EQ(summary.summarize(sample_order()), "ORD-77: 2 line(s)");
}

TEST(StaticDispatchVariantTest, BothExpressionsAgreeOnTheSameOrder) {
    CrtpOrderSummary crtp_summary;
    DeducedOrderSummary deduced_summary;
    const Order order = sample_order();

    EXPECT_EQ(crtp_summary.summarize(order), deduced_summary.summarize(order));
}
#endif

}  // namespace
}  // namespace orderexport
