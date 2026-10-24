#pragma once

#include "comparable_mixin.hpp"
#include "shipment_types.hpp"

namespace crtp21 {

// ShipmentQuote and CarrierRating share nothing except Comparable<Derived>:
// one orders by integer Cents, the other by a floating-point score, and
// neither inherits from the other. That is the proof Comparable is a
// general mixin rather than a RateModel-specific helper: the self-type
// trick works the same way no matter what sort_key() returns.
class ShipmentQuote : public Comparable<ShipmentQuote> {
public:
    constexpr explicit ShipmentQuote(Cents total_cents) : total_cents_(total_cents) {}

    constexpr Cents sort_key() const { return total_cents_; }
    constexpr Cents total_cents() const { return total_cents_; }

private:
    Cents total_cents_;
};

class CarrierRating : public Comparable<CarrierRating> {
public:
    constexpr explicit CarrierRating(double score) : score_(score) {}

    constexpr double sort_key() const { return score_; }
    constexpr double score() const { return score_; }

private:
    double score_;
};

}  // namespace crtp21
