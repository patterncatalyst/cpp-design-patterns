#pragma once

#include <string>

#include "eta_estimate.hpp"

namespace refactor29 {

// Stage one: the inherited ETA estimator exactly as it arrived. No seam
// around "today," no named carrier abstraction, a six-argument call. This
// is the starting point the whole chapter works from.
class LegacyEtaEstimator {
public:
    EtaEstimate estimate(const std::string& carrier,
                          const std::string& origin_zone,
                          const std::string& destination_zone,
                          double weight_kg,
                          bool is_residential,
                          double declared_value) const;
};

}  // namespace refactor29
