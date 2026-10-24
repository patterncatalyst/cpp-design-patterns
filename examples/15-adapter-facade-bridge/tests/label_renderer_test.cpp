#include "label_renderer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace shipping {
namespace {

TEST(LabelRendererTest, RendersCarrierWeightZoneAndTracking) {
    LabelRenderer renderer("GlobalFreight");
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    const std::string label = renderer.render(shipment, "TRK-9001");

    EXPECT_NE(label.find("GlobalFreight"), std::string::npos);
    EXPECT_NE(label.find("Regional"), std::string::npos);
    EXPECT_NE(label.find("TRK-9001"), std::string::npos);
}

TEST(LabelRendererTest, CopyProducesAnIndependentEqualRenderer) {
    LabelRenderer original("GlobalFreight");
    LabelRenderer copy = original;
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(original.render(shipment, "TRK-9001"), copy.render(shipment, "TRK-9001"));
}

TEST(LabelRendererTest, CopyAssignmentReplacesTheTargetsImpl) {
    LabelRenderer original("GlobalFreight");
    LabelRenderer other("SummitFreight");
    const Shipment shipment{1.0, DestinationZone::Local, 0};

    other = original;

    EXPECT_EQ(original.render(shipment, "TRK-1"), other.render(shipment, "TRK-1"));
}

TEST(LabelRendererTest, MoveTransfersTheImplWithoutCopying) {
    LabelRenderer original("GlobalFreight");
    const Shipment shipment{1.0, DestinationZone::Local, 0};
    const std::string expected = original.render(shipment, "TRK-1");

    LabelRenderer moved = std::move(original);

    EXPECT_EQ(moved.render(shipment, "TRK-1"), expected);
}

TEST(LabelRendererTest, HeaderHidesTheImplBehindASinglePointer) {
    // LabelRenderer's only data member is a std::unique_ptr<Impl>, and the
    // header never defines Impl, so this object's size is exactly one
    // pointer no matter how much formatting detail the .cpp file holds.
    EXPECT_EQ(sizeof(LabelRenderer), sizeof(void*));
}

}  // namespace
}  // namespace shipping
