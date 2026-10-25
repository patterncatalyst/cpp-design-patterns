#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "carrier_factory.hpp"
#include "lifecycle_observer.hpp"
#include "shipment.hpp"
#include "shipment_dispatcher.hpp"
#include "shipment_types.hpp"

namespace {

using finalproject31::Cents;
using finalproject31::Shipment;
using finalproject31::ShipmentStatus;

std::string format_cents(Cents cents) {
    std::ostringstream out;
    out << "$" << std::fixed << std::setprecision(2) << (static_cast<double>(cents) / 100.0);
    return out.str();
}

// Observer: a plain console printer standing in for any real subscriber,
// a tracking dashboard, a notification service, a billing ledger. It
// knows nothing about Strategy, Factory, Command, or State; it only reacts
// to the one event Shipment fires.
class ConsolePrintingObserver : public finalproject31::LifecycleObserver {
public:
    void on_transitioned(const Shipment& shipment, ShipmentStatus from, ShipmentStatus to) override {
        std::cout << "  [observer] " << shipment.id() << ": " << finalproject31::to_string(from) << " -> "
                  << finalproject31::to_string(to) << '\n';
    }
};

}  // namespace

int main() {
    using finalproject31::CarrierRequest;
    using finalproject31::ShipmentDispatcher;
    using finalproject31::ShipmentEvent;

    ShipmentDispatcher dispatcher;
    ConsolePrintingObserver observer;

    std::cout << "== Opening a shipment with Skyhaul Express ==\n";
    Shipment& shipment = dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);
    dispatcher.subscribe(observer);
    std::cout << "  carrier: " << dispatcher.carrier_profile().carrier_name
              << " (SLA " << dispatcher.carrier_profile().sla_hours << "h)\n";
    std::cout << "  opening quote at " << shipment.weight_kg() << "kg: " << format_cents(shipment.quoted_rate_cents())
              << '\n';

    std::cout << "\n== Advancing the lifecycle: PickUp, Dispatch ==\n";
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::PickUp));
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::Dispatch));
    std::cout << "  status now: " << finalproject31::to_string(shipment.status()) << '\n';

    std::cout << "\n== A warehouse rescale corrects the weight to 15kg ==\n";
    dispatcher.apply(dispatcher.make_adjust_weight_command(15.0));
    std::cout << "  weight now: " << shipment.weight_kg() << "kg, recomputed quote: "
              << format_cents(shipment.quoted_rate_cents()) << '\n';

    std::cout << "\n== The rescale turns out to be a scanner glitch; undo it ==\n";
    dispatcher.undo_last();
    std::cout << "  weight restored to: " << shipment.weight_kg() << "kg, quote restored to: "
              << format_cents(shipment.quoted_rate_cents()) << '\n';

    std::cout << "\n== Delivering the shipment ==\n";
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::Deliver));
    std::cout << "  status now: " << finalproject31::to_string(shipment.status()) << '\n';

    std::cout << "\n== Operator mis-scanned PickUp again by habit; the console rejects it ==\n";
    try {
        dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::PickUp));
    } catch (const finalproject31::IllegalTransitionError& error) {
        std::cout << "  rejected: " << error.what() << '\n';
    }

    return 0;
}
