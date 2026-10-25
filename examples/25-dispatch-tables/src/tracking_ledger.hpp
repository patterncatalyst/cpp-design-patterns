#pragma once

// The receiver every dispatch form in this example writes into. Handler
// functions depend on TrackingSink, the interface, rather than on
// TrackingLedger directly, so a test can swap in a recording fake or a
// gmock collaborator double without touching a single handler.

#include <string>
#include <unordered_map>
#include <vector>

namespace dispatch25 {

class TrackingSink {
public:
    virtual ~TrackingSink() = default;
    virtual void record(const std::string& tracking_id, const std::string& note) = 0;
};

class TrackingLedger : public TrackingSink {
public:
    void record(const std::string& tracking_id, const std::string& note) override;
    std::vector<std::string> history(const std::string& tracking_id) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> notes_;
};

}  // namespace dispatch25
