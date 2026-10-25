#pragma once

#include <string>

namespace refactor29 {

// Introduce Parameter Object: the six positional arguments LegacyEtaEstimator
// and SeamedEtaEstimator both take collapse into one named value. A caller
// updating one field no longer has to recount commas against the
// declaration, and a new field can be added in one place instead of at
// every call site.
struct ShipmentProfile {
    std::string carrier;
    std::string origin_zone;
    std::string destination_zone;
    double weight_kg = 0.0;
    bool is_residential = false;
    double declared_value = 0.0;
};

}  // namespace refactor29
