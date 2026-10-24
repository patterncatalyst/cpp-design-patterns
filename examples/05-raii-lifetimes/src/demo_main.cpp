// RAII and deterministic lifetimes at a warehouse loading dock, shown
// three ways: a resource guard that leases and releases a dock bay
// (DockBayLease), the same guard surviving an exception thrown mid-batch,
// and a general-purpose scope-exit utility (ScopeGuard) rolling back a
// counter on the same failure path.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <vector>

#include "dock_bay_lease.hpp"
#include "dock_bay_registry.hpp"
#include "labeling_station.hpp"
#include "scope_guard.hpp"

namespace {

void print_bay_state(const warehouse::DockBayRegistry& registry, std::size_t bay_id) {
    std::cout << "  Bay " << bay_id << " is " << (registry.is_leased(bay_id) ? "leased" : "free") << '\n';
}

}  // namespace

int main() {
    using namespace warehouse;

    std::cout << "== Acquire in the constructor, release in the destructor ==\n";
    DockBayRegistry registry(4);
    {
        DockBayLease lease(registry, 0);
        std::cout << "  Lease acquired for bay " << lease.bay_id() << '\n';
        print_bay_state(registry, 0);
    }  // lease destructs here; the bay releases with no explicit call
    std::cout << "  Lease went out of scope.\n";
    print_bay_state(registry, 0);

    std::cout << "\n== A second lease on an already-leased bay is rejected ==\n";
    DockBayLease first_lease(registry, 1);
    try {
        DockBayLease second_lease(registry, 1);
        (void)second_lease;
    } catch (const BayUnavailableError& error) {
        std::cout << "  Rejected: " << error.what() << '\n';
    }

    std::cout << "\n== Move transfers ownership; the moved-from lease releases nothing ==\n";
    const auto release_count_before_move_demo = registry.release_count();
    {
        DockBayLease original(registry, 2);
        DockBayLease moved(std::move(original));
        std::cout << "  original.owns_bay() = " << std::boolalpha << original.owns_bay() << '\n';
        std::cout << "  moved.owns_bay()    = " << moved.owns_bay() << '\n';
    }  // both destruct here; only one of the two performs a real release
    std::cout << "  releases recorded by this block (expect 1, not 2): "
               << (registry.release_count() - release_count_before_move_demo) << '\n';

    std::cout << "\n== A whole batch labels successfully; the bay releases at the end ==\n";
    std::vector<ShipmentItem> good_batch{{"PKG-1001", true}, {"PKG-1002", true}, {"PKG-1003", true}};
    std::size_t labels_printed = 0;
    const auto labeled = label_shipment_batch(registry, 3, good_batch, labels_printed);
    std::cout << "  Labeled " << labeled << " shipments.\n";
    print_bay_state(registry, 3);

    std::cout << "\n== A bad barcode partway through rolls back the count and releases the bay ==\n";
    std::vector<ShipmentItem> bad_batch{{"PKG-2001", true}, {"PKG-2002", false}, {"PKG-2003", true}};
    std::size_t labels_printed_on_failure = 0;
    try {
        label_shipment_batch(registry, 3, bad_batch, labels_printed_on_failure);
    } catch (const InvalidShipmentError& error) {
        std::cout << "  Rejected: " << error.what() << '\n';
        std::cout << "  labels_printed after rollback: " << labels_printed_on_failure << '\n';
    }
    print_bay_state(registry, 3);

    std::cout << "\n== ScopeGuard runs its actions in LIFO order, like stack destructors ==\n";
    std::vector<int> run_order;
    {
        auto guard_a = on_scope_exit([&] { run_order.push_back(1); });
        auto guard_b = on_scope_exit([&] { run_order.push_back(2); });
        auto guard_c = on_scope_exit([&] { run_order.push_back(3); });
        (void)guard_a;
        (void)guard_b;
        (void)guard_c;
    }
    std::cout << "  Run order (expect 3, 2, 1): ";
    for (const auto value : run_order) {
        std::cout << value << ' ';
    }
    std::cout << '\n';

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: acquiring a bay without throwing ==\n";
    // Bay 1 is still held by first_lease, declared earlier and still in
    // scope, so this acquisition is rejected without raising an exception.
    if (const auto rejected = DockBayLease::try_acquire(registry, 1); !rejected.has_value()) {
        std::cout << "  Rejected: " << rejected.error() << '\n';
    }
    // Bay 2 released when original and moved both went out of scope above.
    if (const auto accepted = DockBayLease::try_acquire(registry, 2); accepted.has_value()) {
        std::cout << "  Acquired bay " << accepted->bay_id() << " without an exception.\n";
    }
#endif

    return 0;
}
