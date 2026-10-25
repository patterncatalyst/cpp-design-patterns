#include "tracking_ledger.hpp"

namespace dispatch25 {

void TrackingLedger::record(const std::string& tracking_id, const std::string& note) {
    notes_[tracking_id].push_back(note);
}

std::vector<std::string> TrackingLedger::history(const std::string& tracking_id) const {
    const auto it = notes_.find(tracking_id);
    if (it == notes_.end()) {
        return {};
    }
    return it->second;
}

}  // namespace dispatch25
