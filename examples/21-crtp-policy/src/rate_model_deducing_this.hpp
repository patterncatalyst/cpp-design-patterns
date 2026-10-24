#pragma once

#include "shipment_types.hpp"

#if __cplusplus >= 202302L

namespace crtp21 {

// The same RateModel from rate_model.hpp, rewritten with C++23's deducing
// this. DeducedRateModel is not a template at all: quote() takes an
// explicit object parameter, this const Self& self, and the compiler
// deduces Self as whatever concrete type called quote(), the same
// information rate_model.hpp's Derived template argument carried, with no
// template argument in the inheritance list and no static_cast anywhere
// in this file.
class DeducedRateModel {
public:
    template <typename Self>
    constexpr Cents quote(this const Self& self, const Shipment& shipment) {
        return self.compute(shipment);
    }
};

class DeducedFlatRateModel : public DeducedRateModel {
public:
    constexpr explicit DeducedFlatRateModel(Cents flat_cents) : flat_cents_(flat_cents) {}

    constexpr Cents compute(const Shipment& /*shipment*/) const { return flat_cents_; }

private:
    Cents flat_cents_;
};

static_assert(DeducedFlatRateModel(500).quote(Shipment{/*weight_kg=*/9.0, /*declared_value_cents=*/0}) == 500,
              "DeducedRateModel::quote must resolve to DeducedFlatRateModel::compute at compile time");

}  // namespace crtp21

#endif  // __cplusplus >= 202302L
