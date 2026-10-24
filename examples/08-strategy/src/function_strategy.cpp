#include "function_strategy.hpp"

#include <cmath>
#include <utility>

namespace shipping {

RateFn flat_rate(Cents flat_cents) {
    return [flat_cents](const Shipment& /*shipment*/) { return flat_rate_cost(flat_cents); };
}

RateFn weight_based(Cents base_cents, Cents per_kg_cents) {
    return [base_cents, per_kg_cents](const Shipment& shipment) {
        return weight_based_cost(base_cents, per_kg_cents, shipment.weight_kg);
    };
}

RateFn tiered_zone() {
    return [](const Shipment& shipment) { return tiered_zone_cost(shipment); };
}

RateFn with_insurance_surcharge(RateFn base_rate, double cents_per_declared_dollar) {
    return [base_rate = std::move(base_rate), cents_per_declared_dollar](const Shipment& shipment) {
        const Cents base_cost = base_rate(shipment);
        const double declared_dollars = static_cast<double>(shipment.declared_value_cents) / 100.0;
        const auto surcharge = static_cast<Cents>(std::ceil(declared_dollars * cents_per_declared_dollar));
        return base_cost + surcharge;
    };
}

QuoteBook::QuoteBook(RateFn rate_fn) : rate_fn_(std::move(rate_fn)) {}

void QuoteBook::use(RateFn rate_fn) {
    rate_fn_ = std::move(rate_fn);
}

Cents QuoteBook::quote(const Shipment& shipment) const {
    return rate_fn_(shipment);
}

}  // namespace shipping
