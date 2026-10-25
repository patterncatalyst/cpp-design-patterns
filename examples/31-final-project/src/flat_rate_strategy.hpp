#pragma once

#include "rate_strategy.hpp"

namespace finalproject31 {

// A carrier that charges the same fee no matter what is in the box or how
// far it travels. The weight on the shipment is irrelevant to this
// strategy by design, which is exactly what distinguishes it from
// WeightTieredRateStrategy.
class FlatRateStrategy : public RateStrategy {
public:
    explicit FlatRateStrategy(Cents flat_fee_cents);

    Cents quote(const Shipment& shipment) const override;

private:
    Cents flat_fee_cents_;
};

}  // namespace finalproject31
