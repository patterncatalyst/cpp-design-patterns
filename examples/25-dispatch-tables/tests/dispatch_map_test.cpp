#include "dispatch_map.hpp"

#include <gtest/gtest.h>

#include <string>

#include "mock_tracking_sink.hpp"
#include "recording_tracking_sink.hpp"

namespace dispatch25 {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;

TEST(DispatchMapTest, RoutesEachBuiltInWireTagToItsHandler) {
    RecordingTrackingSink sink;
    MessageDispatchMap dispatch_map;

    dispatch_map.dispatch({"CREATED", "TRACK-1", "order placed"}, sink);
    dispatch_map.dispatch({"DELIVERED", "TRACK-1", "left at door"}, sink);

    ASSERT_EQ(sink.calls().size(), 2u);
    EXPECT_NE(sink.calls()[0].second.find("created"), std::string::npos);
    EXPECT_NE(sink.calls()[1].second.find("delivered"), std::string::npos);
}

TEST(DispatchMapTest, UnknownWireTagFallsBackToTheDefaultHandler) {
    RecordingTrackingSink sink;
    MessageDispatchMap dispatch_map;

    dispatch_map.dispatch({"CUSTOMS_HOLD", "TRACK-3", "n/a"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("unrecognized"), std::string::npos);
}

TEST(DispatchMapTest, LateRegistrationIsDispatchableImmediately) {
    RecordingTrackingSink sink;
    MessageDispatchMap dispatch_map;
    const auto handlers_before = dispatch_map.handler_count();

    // Registered well after construction, which is the whole claim of an
    // open dispatch map: a new carrier's wire tag needs no change to this
    // class and no recompile, just a call that can happen at any point
    // while the program is running.
    dispatch_map.register_handler("CUSTOMS_HOLD", [](const CarrierWireMessage& wire, TrackingSink& recorder) {
        recorder.record(wire.tracking_id, "customs-hold: " + wire.detail);
    });

    EXPECT_EQ(dispatch_map.handler_count(), handlers_before + 1);

    dispatch_map.dispatch({"CUSTOMS_HOLD", "TRACK-4", "awaiting duty payment"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("customs-hold"), std::string::npos);
}

TEST(DispatchMapTest, CallsTheSinkExactlyOnceThroughAMockedCollaborator) {
    MockTrackingSink sink;
    EXPECT_CALL(sink, record(Eq("TRACK-5"), HasSubstr("picked-up"))).Times(1);

    MessageDispatchMap dispatch_map;
    dispatch_map.dispatch({"PICKED_UP", "TRACK-5", "scanned at depot"}, sink);
}

#if __cplusplus >= 202302L

TEST(DispatchMapTest, FlatMapVariantRoutesBuiltInTagsUnderCpp23) {
    RecordingTrackingSink sink;
    FlatMessageDispatchMap dispatch_map;

    dispatch_map.dispatch({"DELIVERED", "TRACK-6", "signed for"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("delivered"), std::string::npos);
}

TEST(DispatchMapTest, FlatMapVariantAcceptsALateRegistrationUnderCpp23) {
    RecordingTrackingSink sink;
    FlatMessageDispatchMap dispatch_map;
    const auto handlers_before = dispatch_map.handler_count();

    dispatch_map.register_handler("CUSTOMS_HOLD", [](const CarrierWireMessage& wire, TrackingSink& recorder) {
        recorder.record(wire.tracking_id, "customs-hold: " + wire.detail);
    });

    EXPECT_EQ(dispatch_map.handler_count(), handlers_before + 1);

    dispatch_map.dispatch({"CUSTOMS_HOLD", "TRACK-7", "awaiting duty payment"}, sink);

    ASSERT_EQ(sink.calls().size(), 1u);
    EXPECT_NE(sink.calls()[0].second.find("customs-hold"), std::string::npos);
}

#endif

}  // namespace
}  // namespace dispatch25
