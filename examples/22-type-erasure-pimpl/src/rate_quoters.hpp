#pragma once

#include <utility>

#include "any_rate_quoter.hpp"
#include "shipping_domain.hpp"

namespace shipping {

// Four concrete quoters with nothing in common but the quote() member
// Quotable requires: no shared base class, no virtual function, no
// inheritance relationship between any pair of them. AnyRateQuoter can
// hold any one of the four.

struct FlatRateQuoter {
    Cents flat_cents;

    Cents quote(const Shipment& /*shipment*/) const { return flat_cents; }
};

struct WeightBasedQuoter {
    Cents base_cents;
    Cents per_kg_cents;

    Cents quote(const Shipment& shipment) const {
        return base_cents + static_cast<Cents>(shipment.weight_kg * static_cast<double>(per_kg_cents));
    }
};

// SurgeQuoter carries mutable state: each call to quote() adds one more
// surge step on top of the last, modeling a quoter that nudges its price
// up the more often a session re-asks it within the same checkout. It
// exists to prove AnyRateQuoter's copy is a genuine deep copy and not a
// shared handle to the same Quoter: copying an AnyRateQuoter built from a
// SurgeQuoter and calling quote() on the copy must never move the
// original's surge counter.
struct SurgeQuoter {
    Cents base_cents;
    Cents surge_step_cents;
    mutable int calls_so_far = 0;

    Cents quote(const Shipment& /*shipment*/) const {
        const Cents price = base_cents + static_cast<Cents>(calls_so_far) * surge_step_cents;
        ++calls_so_far;
        return price;
    }
};

// InsuredQuoter wraps an existing AnyRateQuoter and adds a surcharge
// proportional to the shipment's declared value, the same composition
// Chapter 8's with_insurance_surcharge() built out of a std::function.
// Here the thing being wrapped, and the thing doing the wrapping, are
// both erased behind the same AnyRateQuoter interface, so InsuredQuoter
// itself satisfies Quotable and can be erased a second time, the same
// decorator-without-a-decorator-class result Chapter 16 reaches with
// inheritance instead of erasure.
class InsuredQuoter {
public:
    InsuredQuoter(AnyRateQuoter inner, double cents_per_declared_dollar)
        : inner_(std::move(inner)), cents_per_declared_dollar_(cents_per_declared_dollar) {}

    Cents quote(const Shipment& shipment) const {
        const Cents base_cost = inner_.quote(shipment);
        const double declared_dollars = static_cast<double>(shipment.declared_value_cents) / 100.0;
        const auto surcharge = static_cast<Cents>(declared_dollars * cents_per_declared_dollar_);
        return base_cost + surcharge;
    }

private:
    AnyRateQuoter inner_;
    double cents_per_declared_dollar_;
};

}  // namespace shipping
