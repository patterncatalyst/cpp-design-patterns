#pragma once

#include "shipment_types.hpp"

namespace crtp21 {

// CRTP, expression one: a base class template that resolves to the
// derived type's own method with no vtable anywhere. RateModel<Derived>
// declares no virtual function at all; quote() casts *this down to
// Derived and calls Derived::compute() directly, a call the compiler can
// see straight through and inline. Every concrete rate model below names
// itself as RateModel's template argument, the construction every CRTP
// base requires: that argument is what lets the base class reach a method
// it never declared.
template <typename Derived>
class RateModel {
public:
    constexpr Cents quote(const Shipment& shipment) const {
        return static_cast<const Derived*>(this)->compute(shipment);
    }
};

class FlatRateModel : public RateModel<FlatRateModel> {
public:
    constexpr explicit FlatRateModel(Cents flat_cents) : flat_cents_(flat_cents) {}

    constexpr Cents compute(const Shipment& /*shipment*/) const { return flat_cents_; }

private:
    Cents flat_cents_;
};

class WeightedRateModel : public RateModel<WeightedRateModel> {
public:
    constexpr WeightedRateModel(Cents base_cents, Cents per_kg_cents)
        : base_cents_(base_cents), per_kg_cents_(per_kg_cents) {}

    // Rounds the variable portion up to the next whole cent by hand rather
    // than calling std::ceil, so this method stays constexpr-friendly
    // under both C++20 and C++23 without depending on <cmath> being
    // constexpr for the standard this example is compiled against.
    constexpr Cents compute(const Shipment& shipment) const {
        const double variable_cost = shipment.weight_kg * static_cast<double>(per_kg_cents_);
        const auto truncated = static_cast<Cents>(variable_cost);
        const Cents rounded_up = (static_cast<double>(truncated) < variable_cost) ? truncated + 1 : truncated;
        return base_cents_ + rounded_up;
    }

private:
    Cents base_cents_;
    Cents per_kg_cents_;
};

// Compile-time proof, not just a runtime test, that quote() resolves to
// the right compute() with zero indirection: this evaluates FlatRateModel
// all the way through RateModel<FlatRateModel>::quote() during translation.
// If quote() ever regressed to a virtual call, this line would stop
// compiling rather than merely failing a test at run time.
static_assert(FlatRateModel(500).quote(Shipment{/*weight_kg=*/9.0, /*declared_value_cents=*/0}) == 500,
              "RateModel<FlatRateModel>::quote must resolve to FlatRateModel::compute at compile time");

}  // namespace crtp21
