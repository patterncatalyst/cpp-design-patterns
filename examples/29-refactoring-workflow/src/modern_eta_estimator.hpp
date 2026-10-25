#pragma once

#include <memory>

#include "clock.hpp"
#include "eta_estimate.hpp"
#include "shipment_profile.hpp"

namespace refactor29 {

// Stage three: Fowler's named refactorings, applied now that the clock
// seam and a characterization suite both exist. Extract Function split
// the holiday adjustment and the weight/value surcharge out of one long
// body; Replace Conditional with Polymorphism retired the carrier
// if/else chain; Introduce Parameter Object retired the six-argument
// call. The clock seam from stage two is kept exactly as it was: this
// stage never touches how "today" is obtained, only how the rest of the
// calculation is organized.
class ModernEtaEstimator {
public:
    ModernEtaEstimator();
    explicit ModernEtaEstimator(const Clock& clock);

    EtaEstimate estimate(const ShipmentProfile& shipment) const;

private:
    std::unique_ptr<Clock> owned_clock_;
    const Clock& clock_;
};

}  // namespace refactor29
