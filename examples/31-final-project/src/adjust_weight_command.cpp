#include "adjust_weight_command.hpp"

#include <sstream>
#include <stdexcept>

namespace finalproject31 {

AdjustWeightCommand::AdjustWeightCommand(Shipment& shipment, const RateStrategy& rate_strategy, double new_weight_kg)
    : shipment_(shipment), rate_strategy_(rate_strategy), new_weight_kg_(new_weight_kg) {}

void AdjustWeightCommand::execute() {
    previous_weight_kg_ = shipment_.weight_kg();
    previous_quoted_rate_cents_ = shipment_.quoted_rate_cents();

    shipment_.set_weight_kg(new_weight_kg_);
    shipment_.set_quoted_rate_cents(rate_strategy_.quote(shipment_));
    executed_ = true;
}

void AdjustWeightCommand::undo() {
    if (!executed_) {
        throw std::logic_error("AdjustWeightCommand::undo() called before execute()");
    }
    shipment_.set_weight_kg(previous_weight_kg_);
    shipment_.set_quoted_rate_cents(previous_quoted_rate_cents_);
}

std::string AdjustWeightCommand::description() const {
    std::ostringstream out;
    out << "adjust shipment '" << shipment_.id() << "' weight to " << new_weight_kg_ << "kg";
    return out.str();
}

}  // namespace finalproject31
