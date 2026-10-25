#pragma once

#include <string>

namespace refactor29 {

// The original dispatch logic from the inherited ETA estimator: base
// transit days by carrier and zone match, plus one extra day when a
// shipment's declared value is high enough to require a delivery
// signature. This pair of free functions is pure, has no hidden
// collaborator, and is already testable exactly as it stands, which is
// why the characterization test in this example starts here rather than
// on estimate() as a whole. Stage two (SeamedEtaEstimator) calls these
// same two functions unchanged; only stage three (ModernEtaEstimator)
// replaces them, and only after both are under test.
int base_transit_days(const std::string& carrier,
                       const std::string& origin_zone,
                       const std::string& destination_zone,
                       double declared_value);

// Weight, residential-address, and high-value surcharges. Also pure, and
// also unchanged between stage one and stage two.
double base_surcharge(double weight_kg, bool is_residential, double declared_value);

}  // namespace refactor29
