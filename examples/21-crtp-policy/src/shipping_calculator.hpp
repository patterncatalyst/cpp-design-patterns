#pragma once

#include <concepts>
#include <utility>

#include "shipment_types.hpp"

namespace crtp21 {

// Policy-based design: each policy family is a small, independent type
// with one job, and RoundsCents / SurchargesShipment constrain what a
// template argument in that family must provide, the same job Chapter 8's
// RatePolicy concept does for a single policy parameter. Here there are
// two independent policy slots, composed at compile time by
// ShippingCalculator's two template parameters rather than by a runtime
// field.
template <typename Policy>
concept RoundsCents = requires(const Policy& policy, Cents cents) {
    { policy.round(cents) } -> std::convertible_to<Cents>;
};

template <typename Policy>
concept SurchargesShipment = requires(const Policy& policy, const Shipment& shipment) {
    { policy.surcharge(shipment) } -> std::convertible_to<Cents>;
};

// Rounding policies -------------------------------------------------------

struct NoRoundingPolicy {
    constexpr Cents round(Cents cents) const { return cents; }
};

struct RoundUpToNickelPolicy {
    constexpr Cents round(Cents cents) const {
        const Cents remainder = cents % 5;
        return remainder == 0 ? cents : cents + (5 - remainder);
    }
};

// Surcharge policies --------------------------------------------------------

struct NoSurchargePolicy {
    constexpr Cents surcharge(const Shipment& /*shipment*/) const { return 0; }
};

struct FlatSurchargePolicy {
    Cents flat_cents = 0;

    constexpr Cents surcharge(const Shipment& /*shipment*/) const { return flat_cents; }
};

struct DeclaredValueSurchargePolicy {
    // One percent of the declared value, in integer cents: declared value
    // is already in cents, so dividing by 100 keeps the whole calculation
    // in integer arithmetic with no floating-point rounding to reason
    // about.
    constexpr Cents surcharge(const Shipment& shipment) const { return shipment.declared_value_cents / 100; }
};

// The host: composed at compile time from a RoundsCents policy and a
// SurchargesShipment policy. Swapping either template argument changes
// quote()'s behavior; neither swap touches ShippingCalculator's own code,
// and neither pays for a virtual call or a type-erased callable, because
// the policy types are baked into ShippingCalculator<Rounding, Surcharge>
// before the program ever runs.
template <RoundsCents Rounding, SurchargesShipment Surcharge>
class ShippingCalculator {
public:
    constexpr explicit ShippingCalculator(Cents base_cents, Rounding rounding_policy = Rounding{},
                                           Surcharge surcharge_policy = Surcharge{})
        : base_cents_(base_cents),
          rounding_policy_(std::move(rounding_policy)),
          surcharge_policy_(std::move(surcharge_policy)) {}

    constexpr Cents quote(const Shipment& shipment) const {
        const Cents raw_cents = base_cents_ + surcharge_policy_.surcharge(shipment);
        return rounding_policy_.round(raw_cents);
    }

private:
    Cents base_cents_;
    Rounding rounding_policy_;
    Surcharge surcharge_policy_;
};

// A compile-time proof that the plainest composition, no rounding and no
// surcharge, resolves to exactly the base rate with no runtime step
// involved at all.
static_assert(ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy>(1000).quote(
                  Shipment{/*weight_kg=*/2.0, /*declared_value_cents=*/0}) == 1000,
              "a no-rounding, no-surcharge ShippingCalculator must quote exactly the base rate");

}  // namespace crtp21
