#include "dispatch_table_constexpr.hpp"

#include <gtest/gtest.h>

#include <string>

#include "mock_tracking_sink.hpp"
#include "recording_tracking_sink.hpp"

namespace dispatch25 {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;

TEST(ConstexprDispatchTableTest, EveryMessageTypeHasANonNullTableEntry) {
    // The static_assert in dispatch_table_constexpr.hpp already proved this
    // at compile time; reading kDispatchTable here proves it again, live,
    // at test time, the same way Chapter 23's construction-count assertion
    // backs up a claim with a number instead of a read of the source.
    for (std::size_t index = 0; index < kCarrierMessageTypeCount; ++index) {
        EXPECT_NE(kDispatchTable[index], nullptr) << "missing table entry at index " << index;
    }
}

TEST(ConstexprDispatchTableTest, RoutesEachMessageTypeToItsOwnHandler) {
    RecordingTrackingSink sink;
    const CarrierMessageType types[] = {
        CarrierMessageType::Created,       CarrierMessageType::PickedUp,  CarrierMessageType::InTransit,
        CarrierMessageType::OutForDelivery, CarrierMessageType::Delivered, CarrierMessageType::DeliveryException,
    };
    const std::string expected_substrings[] = {
        "created", "picked-up", "in-transit", "out-for-delivery", "delivered", "delivery-exception",
    };

    for (std::size_t i = 0; i < kCarrierMessageTypeCount; ++i) {
        dispatch_via_constexpr_table({types[i], "TRACK-1", "detail"}, sink);
    }

    ASSERT_EQ(sink.calls().size(), kCarrierMessageTypeCount);
    for (std::size_t i = 0; i < kCarrierMessageTypeCount; ++i) {
        EXPECT_NE(sink.calls()[i].second.find(expected_substrings[i]), std::string::npos)
            << "call " << i << " (\"" << sink.calls()[i].second << "\") missing \"" << expected_substrings[i] << "\"";
    }
}

TEST(ConstexprDispatchTableTest, CallsTheSinkExactlyOnceForAKnownMessageType) {
    MockTrackingSink sink;
    EXPECT_CALL(sink, record(Eq("TRACK-9"), HasSubstr("out-for-delivery"))).Times(1);

    dispatch_via_constexpr_table({CarrierMessageType::OutForDelivery, "TRACK-9", "on van"}, sink);
}

TEST(ConstexprDispatchTableTest, OutOfRangeIndexFallsBackToTheUnrecognizedHandler) {
    RecordingTrackingSink sink;

    dispatch_via_constexpr_table({static_cast<CarrierMessageType>(99), "TRACK-2", "garbled"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("unrecognized"), std::string::npos);
}

}  // namespace
}  // namespace dispatch25
