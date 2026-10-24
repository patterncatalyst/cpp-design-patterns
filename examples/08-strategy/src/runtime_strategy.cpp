#include "runtime_strategy.hpp"

#include <utility>

namespace shipping {

FlatRateCarrier::FlatRateCarrier(Cents flat_cents) : flat_cents_(flat_cents) {}

Cents FlatRateCarrier::quote(const Shipment& /*shipment*/) const {
    return flat_rate_cost(flat_cents_);
}

WeightBasedCarrier::WeightBasedCarrier(Cents base_cents, Cents per_kg_cents)
    : base_cents_(base_cents), per_kg_cents_(per_kg_cents) {}

Cents WeightBasedCarrier::quote(const Shipment& shipment) const {
    return weight_based_cost(base_cents_, per_kg_cents_, shipment.weight_kg);
}

Cents TieredZoneCarrier::quote(const Shipment& shipment) const {
    return tiered_zone_cost(shipment);
}

RateEstimator::RateEstimator(std::unique_ptr<RateStrategy> strategy) : strategy_(std::move(strategy)) {}

void RateEstimator::use_strategy(std::unique_ptr<RateStrategy> strategy) {
    strategy_ = std::move(strategy);
}

Cents RateEstimator::estimate(const Shipment& shipment) const {
    return strategy_->quote(shipment);
}

#if __cplusplus >= 202302L
std::expected<Cents, std::string> RateEstimator::try_estimate(const Shipment& shipment) const {
    if (shipment.weight_kg < 0.0) {
        return std::unexpected<std::string>("shipment weight_kg cannot be negative");
    }
    return strategy_->quote(shipment);
}
#endif

}  // namespace shipping
