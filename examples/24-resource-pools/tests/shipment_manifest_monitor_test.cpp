#include "shipment_manifest_monitor.hpp"

#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace pool24 {
namespace {

TEST(ShipmentManifestMonitorTest, AddItemAndRemoveItemRoundTrip) {
    ShipmentManifestMonitor manifest("SHM-6001");

    manifest.add_item("SKU-1", 2.5);
    manifest.add_item("SKU-2", 3.0);
    EXPECT_EQ(manifest.item_count(), 2u);
    EXPECT_DOUBLE_EQ(manifest.total_weight_kg(), 5.5);

    EXPECT_TRUE(manifest.remove_item("SKU-1"));
    EXPECT_FALSE(manifest.remove_item("SKU-1")) << "removing an already-removed sku must report false, not throw";
    EXPECT_EQ(manifest.item_count(), 1u);
}

// The behavior this test exists to prove: many threads calling add_item()
// concurrently on the SAME manifest must never lose an update. Without the
// monitor's internal mutex serializing every call, two threads racing on
// items_.emplace_back() could corrupt the vector or drop an item; with it,
// the final count is exactly the number of calls made, every time this
// test runs.
TEST(ShipmentManifestMonitorTest, ConcurrentAddItemCallsAreAllSerializedWithoutLoss) {
    constexpr int kThreadCount = 8;
    constexpr int kItemsPerThread = 200;
    ShipmentManifestMonitor manifest("SHM-6002");

    {
        std::vector<std::jthread> scanners;
        scanners.reserve(kThreadCount);
        for (int t = 0; t < kThreadCount; ++t) {
            scanners.emplace_back([&manifest, t] {
                for (int i = 0; i < kItemsPerThread; ++i) {
                    manifest.add_item("SKU-" + std::to_string(t) + "-" + std::to_string(i), 1.0);
                }
            });
        }
        // jthreads join here, at the end of this scope, before any
        // assertion below reads the manifest.
    }

    EXPECT_EQ(manifest.item_count(), static_cast<std::size_t>(kThreadCount * kItemsPerThread));
    EXPECT_DOUBLE_EQ(manifest.total_weight_kg(), static_cast<double>(kThreadCount * kItemsPerThread));
}

TEST(ShipmentManifestMonitorTest, SnapshotReturnsAConsistentCopy) {
    ShipmentManifestMonitor manifest("SHM-6003");
    manifest.add_item("SKU-A", 1.0);
    manifest.add_item("SKU-B", 2.0);

    const auto items = manifest.snapshot();

    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].first, "SKU-A");
    EXPECT_EQ(items[1].first, "SKU-B");
}

}  // namespace
}  // namespace pool24
