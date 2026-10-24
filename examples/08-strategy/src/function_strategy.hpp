#pragma once

#include <functional>

#include "shipping_domain.hpp"

namespace shipping {

// Strategy expression 2: a value-semantic callable. RateFn is simply a
// std::function, so a strategy is any callable with the right signature:
// a free function, a lambda, or a composition of other RateFn values.
// QuoteBook holds one by value and can be reassigned at runtime, the same
// capability RateEstimator gets from swapping a unique_ptr, but without an
// abstract base class in sight.
using RateFn = std::function<Cents(const Shipment&)>;

RateFn flat_rate(Cents flat_cents);
RateFn weight_based(Cents base_cents, Cents per_kg_cents);
RateFn tiered_zone();

// Wraps an existing RateFn with an insurance surcharge proportional to the
// shipment's declared value. This is composing strategies at runtime: an
// abstract-class hierarchy would need a decorator class to get the same
// effect, but a callable composes for free.
RateFn with_insurance_surcharge(RateFn base_rate, double cents_per_declared_dollar);

class QuoteBook {
public:
    explicit QuoteBook(RateFn rate_fn);

    void use(RateFn rate_fn);

    Cents quote(const Shipment& shipment) const;

private:
    RateFn rate_fn_;
};

}  // namespace shipping
