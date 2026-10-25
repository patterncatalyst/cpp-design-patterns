#pragma once

#include <string>
#include <vector>

namespace antipattern26 {

// A receipt that looks like a complete answer. It is not: see
// was_last_calculation_surcharge_applied() below.
struct PricedShipmentReceipt {
    std::string order_id;
    double total_cost = 0.0;
};

// Misuse #2: the god object. ShipmentOperationsManager resolves a rate
// calculator through two singletons, validates its input, logs every
// calculation, and formats a receipt for display: four unrelated
// responsibilities (pricing orchestration, validation, audit logging,
// presentation) owned by one class because each one, on its own, seemed
// too small to deserve its own type. A reviewer asking "what does this
// class do" cannot answer in one sentence, which is the practical test for
// whether a class has taken on too much.
//
// Misuse #4 lives in one specific method here: was_last_calculation_
// surcharge_applied(). It answers a question about "the last calculation,"
// tracked as mutable state on the manager, instead of as a field on the
// PricedShipmentReceipt the calculation already returned. That is a
// leaky abstraction: the manager's public surface does not hide its
// internal, call-order-dependent bookkeeping, it exposes it, and a caller
// has to understand the manager's internals (state updates on every call)
// to use that one method correctly.
class ShipmentOperationsManager {
public:
    PricedShipmentReceipt price_shipment(const std::string& order_id, double base_cost);

    // Leaky: this answers "was a surcharge applied the last time anyone
    // called price_shipment on this object," not "was a surcharge applied
    // to the receipt I am holding." Those are only the same question if no
    // other shipment has been priced in between.
    bool was_last_calculation_surcharge_applied() const;

    const std::vector<std::string>& operation_log() const;

    std::string format_receipt(const PricedShipmentReceipt& receipt) const;

private:
    void validate_base_cost(double base_cost) const;
    void log_event(std::string event);

    std::vector<std::string> log_;
    double last_surcharge_multiplier_ = 1.0;
};

}  // namespace antipattern26
