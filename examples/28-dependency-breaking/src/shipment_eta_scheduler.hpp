#pragma once

// Extract and Override Call. The inherited schedule_shipment() method
// validated a shipment, ran its transit-day arithmetic, and called
// std::chrono::system_clock::now() inline, in the middle of its own body,
// to stamp when the shipment was dispatched. That one call was the only
// reason the method could never be tested deterministically: every run
// produced a different "now," so an assertion on the result could only
// ever check a range, never an exact value.
//
// Extracting the call into its own current_time() method changes nothing
// about when it runs or what it returns in production. It gives a test a
// named seam to override, the same seam Subclass and Override Method
// relies on more generally: a test-only subclass substitutes one method
// and inherits everything else unchanged.

#include <chrono>
#include <string>

namespace breakdep28 {

struct ScheduledEta {
    std::string shipment_id;
    std::chrono::system_clock::time_point dispatched_at;
    std::chrono::system_clock::time_point promised_by;
};

// C++20 form: a protected virtual method, overridden by a test subclass.
class ShipmentEtaScheduler {
public:
    explicit ShipmentEtaScheduler(int transit_days) : transit_days_(transit_days) {}
    virtual ~ShipmentEtaScheduler() = default;

    ScheduledEta schedule_shipment(const std::string& shipment_id) const {
        const auto dispatched_at = current_time();
        const auto promised_by = dispatched_at + std::chrono::days(transit_days_);
        return ScheduledEta{shipment_id, dispatched_at, promised_by};
    }

protected:
    virtual std::chrono::system_clock::time_point current_time() const;

private:
    int transit_days_;
};

#if __cplusplus >= 202302L
// C++23 form: the same technique expressed with an explicit object
// parameter instead of a virtual function. schedule_shipment() is a
// function template taking `this Self&& self`; calling self.current_time()
// resolves against Self's static type at compile time. A derived class
// that declares its own current_time() hides the base one by ordinary
// member-name lookup, the same subclass-and-override substitution a test
// needs, with no vtable, no `virtual`, and no `override` keyword anywhere
// in either class.
class DeducingEtaScheduler {
public:
    explicit DeducingEtaScheduler(int transit_days) : transit_days_(transit_days) {}

    template <typename Self>
    ScheduledEta schedule_shipment(this Self&& self, const std::string& shipment_id) {
        const auto dispatched_at = self.current_time();
        const auto promised_by = dispatched_at + std::chrono::days(self.transit_days_);
        return ScheduledEta{shipment_id, dispatched_at, promised_by};
    }

    std::chrono::system_clock::time_point current_time() const;

protected:
    int transit_days_;
};
#endif

}  // namespace breakdep28
