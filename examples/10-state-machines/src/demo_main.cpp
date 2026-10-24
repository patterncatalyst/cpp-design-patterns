// A shipment lifecycle (Created -> Paid -> InTransit -> Delivered, with
// Cancelled reachable from Created or Paid) modeled four ways: the GoF
// State pattern, a data-driven enum+table FSM, a std::variant+std::visit
// FSM, and a probability-driven stochastic FSM with an injected, seeded
// random engine.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <random>

#include "gof_state.hpp"
#include "shipment_domain.hpp"
#include "stochastic_fsm.hpp"
#include "table_fsm.hpp"
#include "variant_fsm.hpp"

namespace {

void print_status(const char* label, shipment::ShipmentStatus status) {
    std::cout << label << ": " << shipment::to_string(status) << '\n';
}

}  // namespace

int main() {
    using namespace shipment;

    std::cout << "== Expression 1: GoF State pattern ==\n";
    GofStateMachine gof;
    print_status("Start", gof.status());
    gof.pay();
    print_status("After pay()", gof.status());
    gof.ship();
    print_status("After ship()", gof.status());
    gof.deliver();
    print_status("After deliver()", gof.status());

    std::cout << "\n== Expression 2a: enum + transition table FSM ==\n";
    TableShipmentMachine table;
    table.apply(ShipmentEvent::Pay);
    table.apply(ShipmentEvent::Ship);
    table.apply(ShipmentEvent::Deliver);
    print_status("Same event sequence via the table", table.status());

    std::cout << "\n== Expression 2b: std::variant + std::visit FSM ==\n";
    ShipmentVariant state = CreatedTag{};
    state = apply_event(state, ShipmentEvent::Pay);
    state = apply_event(state, ShipmentEvent::Ship);
    state = apply_event(state, ShipmentEvent::Deliver);
    print_status("Same event sequence via the variant", status_of(state));

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed rejection via std::expected ==\n";
    TableShipmentMachine rejected_table;
    if (const auto result = rejected_table.try_apply(ShipmentEvent::Ship); !result.has_value()) {
        std::cout << "Rejected: " << result.error() << '\n';
    }
#endif

    std::cout << "\n== Expression 3: stochastic FSM (seeded for reproducibility) ==\n";
    StochasticShipmentMachine stochastic(std::mt19937(38));
    stochastic.pay();
    stochastic.ship();
    const auto sequence = stochastic.run_to_terminal();
    std::cout << "Seed 38, InTransit outcomes: ";
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << to_string(sequence[i]);
    }
    std::cout << "\nFinal status: " << to_string(stochastic.status()) << '\n';

    return 0;
}
