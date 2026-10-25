#pragma once

#include <gmock/gmock.h>

#include "clock.hpp"

namespace seam27 {

// A GoogleMock stand-in for Clock. ShipmentSlaChecker only ever calls
// now() through the Clock interface, so this mock plugs into the exact
// same constructor the demo binary uses with SystemClock, but lets a test
// dictate precisely what moment "now" is on each call.
class MockClock : public Clock {
public:
    MOCK_METHOD(std::chrono::system_clock::time_point, now, (), (const, override));
};

}  // namespace seam27
