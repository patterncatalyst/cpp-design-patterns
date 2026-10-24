#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "shipment_domain.hpp"

namespace shipment {

// Expression 3: a probability-driven state machine. The deterministic
// events (pay, ship, cancel) resolve exactly like the table FSM above.
// Once a shipment is InTransit, advance() does not take a caller-chosen
// event at all; it draws a weighted outcome from the injected random
// engine instead: delivered most of the time, delayed (stays InTransit)
// occasionally, lost rarely.
//
// The engine is a constructor parameter, not a static or thread-local
// default, specifically so a test can pass a std::mt19937 seeded with a
// fixed value and get the exact same sequence of outcomes on every run.
// A production caller would inject std::mt19937{std::random_device{}()}
// instead; nothing else about the class changes.
class StochasticShipmentMachine {
public:
    explicit StochasticShipmentMachine(std::mt19937 rng, ShipmentStatus initial = ShipmentStatus::Created);

    ShipmentStatus status() const { return status_; }

    void pay();
    void ship();
    void cancel();

    // Draws one weighted outcome for a shipment that is InTransit: 0.85
    // Delivered, 0.10 delayed (stays InTransit), 0.05 Lost. Throws
    // IllegalTransition from any other status.
    ShipmentStatus advance();

    // Calls advance() repeatedly until the shipment reaches a terminal
    // status (Delivered or Lost), returning every status observed along
    // the way, including the terminal one. Throws std::runtime_error if
    // the shipment has not reached a terminal status within max_steps
    // draws, a defensive bound against a pathological RNG stream rather
    // than a case this example's weights can realistically hit.
    std::vector<ShipmentStatus> run_to_terminal(int max_steps = 10'000);

private:
    ShipmentStatus status_;
    std::mt19937 rng_;
};

}  // namespace shipment
