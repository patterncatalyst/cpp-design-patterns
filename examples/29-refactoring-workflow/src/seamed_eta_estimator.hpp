#pragma once

#include <memory>
#include <string>

#include "clock.hpp"
#include "eta_estimate.hpp"

namespace refactor29 {

// Stage two: Extract Interface pulled Clock out of the wall-clock read,
// and Parameterize Constructor added a seam to supply one. The no-argument
// constructor still builds a real SystemClock internally, so every
// existing production call site keeps compiling and keeps behaving
// exactly as before; the new constructor overload is what a test uses to
// substitute a FixedClock. Everything else here, the carrier dispatch and
// the surcharge rules, is untouched: this stage's only job is making
// "today" substitutable.
class SeamedEtaEstimator {
public:
    SeamedEtaEstimator() : owned_clock_(std::make_unique<SystemClock>()), clock_(*owned_clock_) {}
    explicit SeamedEtaEstimator(const Clock& clock) : clock_(clock) {}

    EtaEstimate estimate(const std::string& carrier,
                          const std::string& origin_zone,
                          const std::string& destination_zone,
                          double weight_kg,
                          bool is_residential,
                          double declared_value) const;

private:
    std::unique_ptr<Clock> owned_clock_;  // empty when a test supplies its own clock
    const Clock& clock_;
};

}  // namespace refactor29
