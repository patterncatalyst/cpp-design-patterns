#pragma once

#include <gmock/gmock.h>

#include "tracking_ledger.hpp"

namespace dispatch25 {

// A GoogleMock stand-in for TrackingLedger. Every dispatch form in this
// example only ever calls record() through the TrackingSink interface, so
// this mock plugs into the same functions the demo binary calls with a
// real TrackingLedger, but lets a test verify exactly how a dispatch form
// calls its collaborator: how many times, and with what arguments.
class MockTrackingSink : public TrackingSink {
public:
    MOCK_METHOD(void, record, (const std::string& tracking_id, const std::string& note), (override));
};

}  // namespace dispatch25
