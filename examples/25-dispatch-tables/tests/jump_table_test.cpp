#include "jump_table.hpp"

#include <gtest/gtest.h>

#include <string>

#include "mock_tracking_sink.hpp"
#include "recording_tracking_sink.hpp"

namespace dispatch25 {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;

TEST(JumpTableTest, EveryMessageTypeHasANonNullTableEntry) {
    for (std::size_t index = 0; index < kCarrierMessageTypeCount; ++index) {
        EXPECT_NE(g_jump_table[index], nullptr) << "missing jump table entry at index " << index;
    }
}

TEST(JumpTableTest, RoutesEachMessageTypeToItsOwnHandler) {
    RecordingTrackingSink sink;
    const CarrierMessageType types[] = {
        CarrierMessageType::Created,       CarrierMessageType::PickedUp,  CarrierMessageType::InTransit,
        CarrierMessageType::OutForDelivery, CarrierMessageType::Delivered, CarrierMessageType::DeliveryException,
    };
    const std::string expected_substrings[] = {
        "created", "picked-up", "in-transit", "out-for-delivery", "delivered", "delivery-exception",
    };

    for (std::size_t i = 0; i < kCarrierMessageTypeCount; ++i) {
        dispatch_via_jump_table({types[i], "TRACK-1", "detail"}, sink);
    }

    ASSERT_EQ(sink.calls().size(), kCarrierMessageTypeCount);
    for (std::size_t i = 0; i < kCarrierMessageTypeCount; ++i) {
        EXPECT_NE(sink.calls()[i].second.find(expected_substrings[i]), std::string::npos)
            << "call " << i << " (\"" << sink.calls()[i].second << "\") missing \"" << expected_substrings[i] << "\"";
    }
}

TEST(JumpTableTest, CallsTheSinkExactlyOnceForAKnownMessageType) {
    MockTrackingSink sink;
    EXPECT_CALL(sink, record(Eq("TRACK-9"), HasSubstr("delivered"))).Times(1);

    dispatch_via_jump_table({CarrierMessageType::Delivered, "TRACK-9", "left on porch"}, sink);
}

TEST(JumpTableTest, OutOfRangeIndexFallsBackToTheUnrecognizedHandler) {
    RecordingTrackingSink sink;

    dispatch_via_jump_table({static_cast<CarrierMessageType>(99), "TRACK-2", "garbled"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("unrecognized"), std::string::npos);
}

}  // namespace
}  // namespace dispatch25
