#pragma once

#include <string>
#include <utility>
#include <vector>

#include "tracking_ledger.hpp"

namespace dispatch25 {

// A plain fake, not a mock: it records every call instead of verifying one
// against expectations set up front. Used where a test wants to read the
// recorded notes back and inspect their content, which gmock's EXPECT_CALL
// matchers can do one call at a time but not as naturally across a whole
// sequence of calls.
class RecordingTrackingSink : public TrackingSink {
public:
    void record(const std::string& tracking_id, const std::string& note) override {
        calls_.emplace_back(tracking_id, note);
    }

    const std::vector<std::pair<std::string, std::string>>& calls() const { return calls_; }

private:
    std::vector<std::pair<std::string, std::string>> calls_;
};

}  // namespace dispatch25
