#pragma once

#include <concepts>
#include <utility>

#include "shipping_domain.hpp"

namespace shipping {

// Strategy expression 3: a compile-time policy. RatePolicy constrains the
// template parameter to anything that can quote a Shipment; StaticRateCalculator
// then picks its pricing behavior at compile time via the template
// argument. There is no virtual call and no type-erased callable at the
// call site, at the cost of the strategy no longer being swappable once
// the program is built.
template <typename Policy>
concept RatePolicy = requires(const Policy& policy, const Shipment& shipment) {
    { policy.quote(shipment) } -> std::convertible_to<Cents>;
};

struct FlatRatePolicy {
    Cents flat_cents;

    Cents quote(const Shipment& /*shipment*/) const { return flat_rate_cost(flat_cents); }
};

struct WeightBasedPolicy {
    Cents base_cents;
    Cents per_kg_cents;

    Cents quote(const Shipment& shipment) const {
        return weight_based_cost(base_cents, per_kg_cents, shipment.weight_kg);
    }
};

struct TieredZonePolicy {
    Cents quote(const Shipment& shipment) const { return tiered_zone_cost(shipment); }
};

template <RatePolicy Policy>
class StaticRateCalculator {
public:
    explicit StaticRateCalculator(Policy policy) : policy_(std::move(policy)) {}

    Cents estimate(const Shipment& shipment) const { return policy_.quote(shipment); }

private:
    Policy policy_;
};

}  // namespace shipping
