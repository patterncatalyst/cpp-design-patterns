#pragma once

#include "rate_strategy.hpp"
#include "shipment.hpp"
#include "shipment_command.hpp"

namespace finalproject31 {

// Where Command meets Strategy: a corrected scale reading changes a
// shipment's weight, and a weight change means the quoted rate is now
// wrong under whichever RateStrategy the carrier's contract calls for.
// execute() asks that same RateStrategy for a fresh quote instead of
// leaving the stale number in place; undo() restores the exact snapshot
// taken before the correction, so it never needs to ask the strategy for
// anything at all.
class AdjustWeightCommand : public ShipmentCommand {
public:
    AdjustWeightCommand(Shipment& shipment, const RateStrategy& rate_strategy, double new_weight_kg);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    Shipment& shipment_;
    const RateStrategy& rate_strategy_;
    double new_weight_kg_;
    double previous_weight_kg_ = 0.0;
    Cents previous_quoted_rate_cents_ = 0;
    bool executed_ = false;
};

}  // namespace finalproject31
