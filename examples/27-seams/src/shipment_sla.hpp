#pragma once

#include <chrono>
#include <string>

namespace seam27 {

// A shipment's service-level agreement: when it was promised by, keyed by
// tracking ID. Deliberately a plain value with no behavior of its own; the
// decision of whether the deadline has passed belongs to a checker, not to
// this struct, so the checker is the only place that decision ever needs
// to change.
struct ShipmentSla {
    std::string tracking_id;
    std::chrono::system_clock::time_point deadline;
};

}  // namespace seam27
