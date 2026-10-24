#include "stochastic_fsm.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace shipment {

namespace {

// Weighted outcomes for one InTransit step. These are the same three
// weights the chapter's diagram calls out on InTransit.
constexpr std::array<double, 3> kInTransitWeights = {0.85, 0.10, 0.05};

ShipmentStatus outcome_for_index(int index) {
    switch (index) {
        case 0: return ShipmentStatus::Delivered;
        case 1: return ShipmentStatus::InTransit;
        default: return ShipmentStatus::Lost;
    }
}

}  // namespace

StochasticShipmentMachine::StochasticShipmentMachine(std::mt19937 rng, ShipmentStatus initial)
    : status_(initial), rng_(std::move(rng)) {}

void StochasticShipmentMachine::pay() {
    if (status_ != ShipmentStatus::Created) {
        throw IllegalTransition(status_, ShipmentEvent::Pay);
    }
    status_ = ShipmentStatus::Paid;
}

void StochasticShipmentMachine::ship() {
    if (status_ != ShipmentStatus::Paid) {
        throw IllegalTransition(status_, ShipmentEvent::Ship);
    }
    status_ = ShipmentStatus::InTransit;
}

void StochasticShipmentMachine::cancel() {
    if (status_ != ShipmentStatus::Created && status_ != ShipmentStatus::Paid) {
        throw IllegalTransition(status_, ShipmentEvent::Cancel);
    }
    status_ = ShipmentStatus::Cancelled;
}

ShipmentStatus StochasticShipmentMachine::advance() {
    if (status_ != ShipmentStatus::InTransit) {
        throw IllegalTransition(status_, ShipmentEvent::Advance);
    }
    std::discrete_distribution<int> distribution(kInTransitWeights.begin(), kInTransitWeights.end());
    const int index = distribution(rng_);
    status_ = outcome_for_index(index);
    return status_;
}

std::vector<ShipmentStatus> StochasticShipmentMachine::run_to_terminal(int max_steps) {
    std::vector<ShipmentStatus> sequence;
    int steps = 0;
    while (status_ == ShipmentStatus::InTransit) {
        if (steps >= max_steps) {
            throw std::runtime_error("stochastic shipment machine did not reach a terminal status within the step bound");
        }
        sequence.push_back(advance());
        ++steps;
    }
    return sequence;
}

}  // namespace shipment
