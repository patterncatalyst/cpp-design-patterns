#pragma once

#include <mutex>

namespace antipattern26 {

// Misuse #1: Singleton as global mutable state.
//
// A thread-safe Meyers singleton, exactly as correct as Chapter 14's
// ShipmentAuditLog. The mechanism is not the bug. The bug is what this
// chapter's over-patterned design does with it: it stores a value, the fuel
// surcharge multiplier, that two independent parts of the pricing pipeline
// each believe they can configure on their own. Only one multiplier exists,
// process-wide, and every reader sees whatever the most recent writer left
// behind, no matter how far apart "writer" and "reader" are in the call
// graph.
class ShippingRateRegistry {
public:
    static ShippingRateRegistry& instance();

    ShippingRateRegistry(const ShippingRateRegistry&) = delete;
    ShippingRateRegistry& operator=(const ShippingRateRegistry&) = delete;

    void set_surcharge_multiplier(double multiplier);
    double surcharge_multiplier() const;

private:
    ShippingRateRegistry() = default;

    mutable std::mutex mutex_;
    double surcharge_multiplier_ = 1.0;
};

}  // namespace antipattern26
