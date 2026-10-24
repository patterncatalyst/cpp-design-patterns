#include "shipment_audit_log_singleton.hpp"

#include <gtest/gtest.h>

namespace shipping::audit {
namespace {

TEST(ShipmentAuditLogSingletonTest, InstanceReturnsTheSameObjectEveryTime) {
    ShipmentAuditLog& first = ShipmentAuditLog::instance();
    ShipmentAuditLog& second = ShipmentAuditLog::instance();

    EXPECT_EQ(&first, &second);
}

TEST(ShipmentAuditLogSingletonTest, RecordsFromAnyCallSiteLandInOneSharedLog) {
    ShipmentAuditLog::instance().reset_for_testing();

    // Two call sites, standing in for two unrelated modules, both reach
    // into the same global accessor with no constructor parameter
    // announcing the dependency to either one.
    ShipmentAuditLog::instance().record("[warehouse] packed ORD-9001");
    ShipmentAuditLog::instance().record("[billing] charged ORD-9001");

    const auto entries = ShipmentAuditLog::instance().entries();
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries.at(0), "[warehouse] packed ORD-9001");
    EXPECT_EQ(entries.at(1), "[billing] charged ORD-9001");
}

TEST(ShipmentAuditLogSingletonTest, ResetForTestingIsTheOnlyWayToIsolateATest) {
    ShipmentAuditLog::instance().record("[warehouse] packed ORD-9002");
    ASSERT_FALSE(ShipmentAuditLog::instance().entries().empty());

    // Without this test-only escape hatch, this test would see every entry
    // left behind by whichever test ran before it. The hatch exists purely
    // to compensate for global mutable state; a type with no global state
    // would never need one.
    ShipmentAuditLog::instance().reset_for_testing();

    EXPECT_TRUE(ShipmentAuditLog::instance().entries().empty());
}

}  // namespace
}  // namespace shipping::audit
