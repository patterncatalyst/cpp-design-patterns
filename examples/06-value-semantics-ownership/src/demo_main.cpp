#include <cstdio>
#include <memory>

#include "carrier_contract.hpp"
#include "dock_slot.hpp"
#include "manifest_tree.hpp"
#include "parcel.hpp"
#include "rate_table_adapter.hpp"
#include "shipping_route.hpp"

using namespace warehouse;

namespace {

void print_money(const char* label, Cents cents) {
    std::printf("%s: $%lld.%02lld\n", label, static_cast<long long>(cents / 100), static_cast<long long>(cents % 100));
}

}  // namespace

int main() {
    std::printf("== Value semantics: a Parcel copy is independent ==\n");
    Parcel original{"TRK-1001", 4.2, "Fragile - this side up"};
    Parcel copy = original;
    copy.weight_kg = 9.9;
    copy.label = "Re-weighed at the hub";
    std::printf("original: %s, %.1f kg, \"%s\"\n", original.tracking_id.c_str(), original.weight_kg, original.label.c_str());
    std::printf("copy:     %s, %.1f kg, \"%s\"\n", copy.tracking_id.c_str(), copy.weight_kg, copy.label.c_str());

    std::printf("\n== Sole ownership: unique_ptr transfers a Parcel between docks ==\n");
    DockSlot inbound;
    DockSlot outbound;
    inbound.assign(std::make_unique<Parcel>(Parcel{"TRK-2002", 1.8, "Standard"}));
    std::printf("before transfer: inbound occupied=%s, outbound occupied=%s\n", inbound.occupied() ? "yes" : "no",
                outbound.occupied() ? "yes" : "no");
    transfer_parcel(inbound, outbound);
    std::printf("after transfer:  inbound occupied=%s, outbound occupied=%s (%s)\n", inbound.occupied() ? "yes" : "no",
                outbound.occupied() ? "yes" : "no", outbound.peek()->tracking_id.c_str());

    std::printf("\n== Shared ownership: two routes quote against one CarrierContract ==\n");
    const Cents tiers[] = {250, 400, 650};
    auto contract = std::make_shared<CarrierContract>("northwind", tiers, 3);
    ShippingRoute north("north-corridor", contract, /*tier=*/1);
    ShippingRoute south("south-corridor", contract, /*tier=*/1);
    print_money("north-corridor quote (before renegotiation)", north.quote());
    print_money("south-corridor quote (before renegotiation)", south.quote());
    contract->set_tier_rate(1, 475);
    print_money("north-corridor quote (after renegotiation)", north.quote());
    print_money("south-corridor quote (after renegotiation)", south.quote());

    std::printf("\n== Breaking a cycle: weak_ptr observes a manifest's parent ==\n");
    auto root = make_root_manifest("M-1000");
    auto child = root->add_child("M-1000-A");
    if (auto locked_parent = child->parent().lock()) {
        std::printf("child's parent while root is alive: %s\n", locked_parent->manifest_id().c_str());
    }
    root.reset();
    std::printf("child's parent after root is reset: %s\n", child->parent().expired() ? "expired" : "still alive");

    std::printf("\n== C++23 refinement: adopting a legacy C factory's pointer ==\n");
    if (auto table = open_rate_table("cardinal")) {
        std::printf("cardinal per-kg rate: %.0f cents (handle releases it automatically)\n", table->per_kg_rate_cents);
    }
    if (!open_rate_table("unknown-carrier")) {
        std::printf("unknown-carrier: rejected, no handle allocated\n");
    }

    return 0;
}
