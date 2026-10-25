#pragma once

#include <chrono>
#include <functional>

#include "clock.hpp"
#include "shipment_sla.hpp"

#if __cplusplus >= 202302L
#include <expected>
#else
#include <optional>
#endif

namespace seam27 {

// ShipmentSlaChecker is LegacyShipmentSlaChecker refactored through the
// object seam: the clock is a constructor argument instead of a call
// buried in the method body. The constructor takes a Clock by reference
// rather than owning one, so a caller supplies a SystemClock in
// production and a MockClock (or any other Clock implementation) in a
// test, with nothing about this class's own code changing between the
// two.
//
// is_breached() also carries a deliberate behavior change from the
// legacy checker: it treats a deadline exactly equal to "now" as
// breached (>=), where the legacy checker treated it as not yet breached
// (>). That boundary could not even be characterized against the legacy
// checker, because no test can force the real wall clock to land on an
// exact instant; the object seam is what makes the boundary testable at
// all, and shipment_sla_checker_test.cpp pins the corrected rule with a
// substituted clock.
class ShipmentSlaChecker {
public:
    explicit ShipmentSlaChecker(const Clock& clock) : clock_(clock) {}

    bool is_breached(const ShipmentSla& sla) const;

#if __cplusplus >= 202302L
    // C++23: report not just whether the deadline passed but by how much,
    // with the overdue amount carried in the error channel instead of
    // discarded. A caller that only needs the yes/no answer still has
    // is_breached(); this is for callers, such as a billing job, that
    // need the magnitude too.
    std::expected<std::chrono::minutes, std::chrono::minutes> try_time_remaining(const ShipmentSla& sla) const;
#else
    // C++20: std::optional can only say a deadline has passed, not by how
    // much; the overdue amount is discarded the moment the deadline
    // slips.
    std::optional<std::chrono::minutes> try_time_remaining(const ShipmentSla& sla) const;
#endif

private:
    std::reference_wrapper<const Clock> clock_;
};

}  // namespace seam27
