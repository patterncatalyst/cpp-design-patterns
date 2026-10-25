#pragma once

#include <vector>

#include "rate_strategy.hpp"

namespace finalproject31 {

// One weight bracket: any shipment at or under max_weight_kg is billed a
// flat base plus a per-kilogram rate for that bracket.
struct WeightTier {
    double max_weight_kg;
    Cents base_cents;
    double cents_per_kg;
};

// A carrier that prices by weight bracket rather than a single flat fee.
// Tiers are checked in the order given; a shipment heavier than every
// bracket's ceiling is billed under the last (heaviest) tier, so the
// table only needs an upper bound for every bracket except the last.
class WeightTieredRateStrategy : public RateStrategy {
public:
    explicit WeightTieredRateStrategy(std::vector<WeightTier> tiers);

    Cents quote(const Shipment& shipment) const override;

private:
    std::vector<WeightTier> tiers_;
};

}  // namespace finalproject31
