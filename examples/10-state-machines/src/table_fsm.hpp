#pragma once

#include <cstddef>
#include <unordered_map>

#include "shipment_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#include <string>
#endif

namespace shipment {

// Expression 2a: a data-driven finite state machine. Every legal
// transition is one row in a table, (from, event) -> to; TableShipmentMachine
// just looks up the current status and event pair and holds no per-status
// branching code of its own. A reviewer can read the whole rule set as
// data, laid out once, rather than as control flow scattered across types.
struct ShipmentTransitionKey {
    ShipmentStatus from;
    ShipmentEvent event;

    friend bool operator==(const ShipmentTransitionKey&, const ShipmentTransitionKey&) = default;
};

struct ShipmentTransitionKeyHash {
    std::size_t operator()(const ShipmentTransitionKey& key) const noexcept;
};

using ShipmentTransitionTable =
    std::unordered_map<ShipmentTransitionKey, ShipmentStatus, ShipmentTransitionKeyHash>;

const ShipmentTransitionTable& shipment_transition_table();

class TableShipmentMachine {
public:
    TableShipmentMachine();

    ShipmentStatus status() const { return status_; }

    // Looks up (status(), event) in the transition table and throws
    // IllegalTransition if no row matches.
    void apply(ShipmentEvent event);

#if __cplusplus >= 202302L
    // C++23 refinement: report a rejected event as a typed error instead
    // of throwing, so a caller on a hot path (a checkout API handling many
    // requests) does not need a try block just to learn an event was
    // rejected. Guarded so the mandatory C++20 build path never needs
    // std::expected.
    std::expected<ShipmentStatus, std::string> try_apply(ShipmentEvent event);
#endif

private:
    ShipmentStatus status_;
};

}  // namespace shipment
