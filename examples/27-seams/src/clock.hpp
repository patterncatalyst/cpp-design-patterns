#pragma once

#include <chrono>

namespace seam27 {

// Clock is the object seam this chapter builds: a small interface standing
// in for "the current moment" wherever inherited code used to call
// std::chrono::system_clock::now() directly. Anything that needs the time
// takes a Clock instead of asking the system clock itself, so a test can
// hand it a substitute that answers with whatever moment the test needs.
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::chrono::system_clock::time_point now() const = 0;
};

// SystemClock is the only implementation the shipped binary ever
// constructs: a one-line forward to the real wall clock, kept behind the
// seam so production code pays no cost beyond one virtual call.
class SystemClock final : public Clock {
public:
    std::chrono::system_clock::time_point now() const override;
};

}  // namespace seam27
