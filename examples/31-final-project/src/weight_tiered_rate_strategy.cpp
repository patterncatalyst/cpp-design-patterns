#include "weight_tiered_rate_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace finalproject31 {

WeightTieredRateStrategy::WeightTieredRateStrategy(std::vector<WeightTier> tiers) : tiers_(std::move(tiers)) {
    if (tiers_.empty()) {
        throw std::invalid_argument("WeightTieredRateStrategy requires at least one tier");
    }
}

Cents WeightTieredRateStrategy::quote(const Shipment& shipment) const {
    const double weight_kg = shipment.weight_kg();
    const WeightTier* selected = &tiers_.back();
    for (const WeightTier& tier : tiers_) {
        if (weight_kg <= tier.max_weight_kg) {
            selected = &tier;
            break;
        }
    }
    return selected->base_cents + static_cast<Cents>(std::llround(selected->cents_per_kg * weight_kg));
}

}  // namespace finalproject31
