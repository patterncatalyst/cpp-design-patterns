#include "labeling_station.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "dock_bay_lease.hpp"
#include "dock_bay_registry.hpp"

namespace warehouse {
namespace {

TEST(LabelShipmentBatchTest, LabelsEveryItemAndReleasesTheBayOnSuccess) {
    DockBayRegistry registry(1);
    const std::vector<ShipmentItem> items{
        {"PKG-1001", true},
        {"PKG-1002", true},
        {"PKG-1003", true},
    };
    std::size_t labels_printed = 0;

    const auto labeled = label_shipment_batch(registry, 0, items, labels_printed);

    EXPECT_EQ(labeled, 3u);
    EXPECT_EQ(labels_printed, 3u);
    EXPECT_FALSE(registry.is_leased(0));
}

TEST(LabelShipmentBatchTest, RollsBackTheCountAndReleasesTheBayWhenABarcodeIsInvalid) {
    DockBayRegistry registry(1);
    const std::vector<ShipmentItem> items{
        {"PKG-2001", true},
        {"PKG-2002", false},
        {"PKG-2003", true},
    };
    std::size_t labels_printed = 0;

    EXPECT_THROW(label_shipment_batch(registry, 0, items, labels_printed), InvalidShipmentError);

    // The ScopeGuard rolled the out-parameter back to zero even though
    // one item labeled successfully before the throw.
    EXPECT_EQ(labels_printed, 0u);
    // The DockBayLease released the bay during stack unwinding, the same
    // as it would on a normal return.
    EXPECT_FALSE(registry.is_leased(0));
}

TEST(LabelShipmentBatchTest, ThrowsBayUnavailableInsteadOfLabelingWhenTheBayIsAlreadyLeased) {
    DockBayRegistry registry(1);
    DockBayLease holder(registry, 0);
    const std::vector<ShipmentItem> items{{"PKG-3001", true}};
    std::size_t labels_printed = 0;

    EXPECT_THROW(label_shipment_batch(registry, 0, items, labels_printed), BayUnavailableError);

    EXPECT_EQ(labels_printed, 0u);
    // The bay is still held by the pre-existing holder, not released by
    // the failed attempt, which never actually acquired it.
    EXPECT_TRUE(registry.is_leased(0));
}

}  // namespace
}  // namespace warehouse
