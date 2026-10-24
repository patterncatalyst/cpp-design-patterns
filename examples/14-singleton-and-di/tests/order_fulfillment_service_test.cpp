#include "order_fulfillment_service.hpp"

#include <gtest/gtest.h>

#include "audit_sink.hpp"
#include "fakes.hpp"

namespace shipping::audit {
namespace {

TEST(OrderFulfillmentServiceTest, FulfillOrderRecordsTheExactAuditMessage) {
    FakeAuditSink sink;
    OrderFulfillmentService service(sink);

    const FulfillmentReceipt receipt = service.fulfill_order("ORD-1");

    EXPECT_EQ(receipt.audit_message, "Order ORD-1 fulfilled");
    ASSERT_EQ(sink.recorded().size(), 1U);
    EXPECT_EQ(sink.recorded().front(), "Order ORD-1 fulfilled");
}

TEST(OrderFulfillmentServiceTest, TwoServiceInstancesNeverShareState) {
    FakeAuditSink sink_a;
    FakeAuditSink sink_b;
    OrderFulfillmentService service_a(sink_a);
    OrderFulfillmentService service_b(sink_b);

    service_a.fulfill_order("ORD-2");
    service_b.fulfill_order("ORD-3");

    // Proof the singleton version cannot offer without a test-only reset:
    // each service's sink holds exactly its own order, with no shared log
    // to isolate between them.
    ASSERT_EQ(sink_a.recorded().size(), 1U);
    ASSERT_EQ(sink_b.recorded().size(), 1U);
    EXPECT_EQ(sink_a.recorded().front(), "Order ORD-2 fulfilled");
    EXPECT_EQ(sink_b.recorded().front(), "Order ORD-3 fulfilled");
}

TEST(OrderFulfillmentServiceTest, FulfillOrderAppendsEachCallInOrder) {
    FakeAuditSink sink;
    OrderFulfillmentService service(sink);

    service.fulfill_order("ORD-7");
    service.fulfill_order("ORD-8");

    ASSERT_EQ(sink.recorded().size(), 2U);
    EXPECT_EQ(sink.recorded().at(0), "Order ORD-7 fulfilled");
    EXPECT_EQ(sink.recorded().at(1), "Order ORD-8 fulfilled");
}

TEST(OrderFulfillmentServiceTest, FulfillOrderWorksAgainstTheRealInMemorySink) {
    InMemoryAuditSink sink;
    OrderFulfillmentService service(sink);

    service.fulfill_order("ORD-4");

    ASSERT_EQ(sink.entries().size(), 1U);
    EXPECT_EQ(sink.entries().front(), "Order ORD-4 fulfilled");
}

#if __cplusplus >= 202302L
TEST(OrderFulfillmentServiceTest, TryFulfillOrderReportsATypedErrorUnderCpp23) {
    FailingAuditSink sink;
    OrderFulfillmentService service(sink);

    const auto result = service.try_fulfill_order("ORD-5");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "audit sink failed to record order ORD-5");
}

TEST(OrderFulfillmentServiceTest, TryFulfillOrderSucceedsWhenTheSinkAccepts) {
    FakeAuditSink sink;
    OrderFulfillmentService service(sink);

    const auto result = service.try_fulfill_order("ORD-6");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->order_id, "ORD-6");
}
#else
TEST(OrderFulfillmentServiceTest, TryFulfillOrderReturnsNulloptUnderCpp20) {
    FailingAuditSink sink;
    OrderFulfillmentService service(sink);

    const auto result = service.try_fulfill_order("ORD-5");

    EXPECT_FALSE(result.has_value());
}

TEST(OrderFulfillmentServiceTest, TryFulfillOrderSucceedsWhenTheSinkAccepts) {
    FakeAuditSink sink;
    OrderFulfillmentService service(sink);

    const auto result = service.try_fulfill_order("ORD-6");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->order_id, "ORD-6");
}
#endif

}  // namespace
}  // namespace shipping::audit
