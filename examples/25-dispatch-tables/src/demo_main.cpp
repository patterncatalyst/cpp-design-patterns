#include <iostream>

#include "dispatch_map.hpp"
#include "dispatch_table_constexpr.hpp"
#include "jump_table.hpp"
#include "tracking_ledger.hpp"

namespace {

void print_history(const dispatch25::TrackingLedger& ledger, const std::string& tracking_id) {
    for (const auto& note : ledger.history(tracking_id)) {
        std::cout << "  " << tracking_id << ": " << note << '\n';
    }
}

}  // namespace

int main() {
    using namespace dispatch25;

    TrackingLedger ledger;

    std::cout << "== Form one: enum-indexed jump table ==\n";
    dispatch_via_jump_table({CarrierMessageType::Created, "SH-1001", "order placed with FastFreight"}, ledger);
    dispatch_via_jump_table({CarrierMessageType::PickedUp, "SH-1001", "scanned at origin depot"}, ledger);
    dispatch_via_jump_table({CarrierMessageType::Delivered, "SH-1001", "left on porch"}, ledger);
    dispatch_via_jump_table({static_cast<CarrierMessageType>(99), "SH-1001", "garbled wire value"}, ledger);
    print_history(ledger, "SH-1001");

    std::cout << "\n== Form two: dispatch map, open and extensible ==\n";
    MessageDispatchMap dispatch_map;
    dispatch_map.dispatch({"CREATED", "SH-2002", "order placed with RegionalParcel"}, ledger);
    dispatch_map.dispatch({"IN_TRANSIT", "SH-2002", "departed regional hub"}, ledger);
    std::cout << "Built-in wire tags registered: " << dispatch_map.handler_count() << '\n';

    // A third carrier, onboarded after this program already started,
    // registers a wire tag the built-in set above never named.
    dispatch_map.register_handler("CUSTOMS_HOLD", [](const CarrierWireMessage& wire, TrackingSink& sink) {
        sink.record(wire.tracking_id, "customs-hold: " + wire.detail);
    });
    dispatch_map.dispatch({"CUSTOMS_HOLD", "SH-2002", "awaiting duty payment"}, ledger);
    dispatch_map.dispatch({"UNKNOWN_EVENT", "SH-2002", "n/a"}, ledger);
    std::cout << "Wire tags registered after onboarding: " << dispatch_map.handler_count() << '\n';
    print_history(ledger, "SH-2002");

    std::cout << "\n== Form three: constexpr std::array table ==\n";
    dispatch_via_constexpr_table({CarrierMessageType::OutForDelivery, "SH-3003", "on van, last stop of the route"}, ledger);
    dispatch_via_constexpr_table({CarrierMessageType::DeliveryException, "SH-3003", "recipient refused delivery"}, ledger);
    print_history(ledger, "SH-3003");

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: the open map as a std::flat_map ==\n";
    FlatMessageDispatchMap flat_dispatch_map;
    flat_dispatch_map.dispatch({"DELIVERED", "SH-4004", "signed for by R. Alvarez"}, ledger);
    flat_dispatch_map.register_handler("CUSTOMS_HOLD", [](const CarrierWireMessage& wire, TrackingSink& sink) {
        sink.record(wire.tracking_id, "customs-hold: " + wire.detail);
    });
    flat_dispatch_map.dispatch({"CUSTOMS_HOLD", "SH-4004", "awaiting duty payment"}, ledger);
    print_history(ledger, "SH-4004");
#endif

    return 0;
}
