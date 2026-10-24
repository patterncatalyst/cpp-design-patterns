// A pallet built from cartons built from items, priced and counted through
// one uniform Composite interface; a handful of shared SkuDescriptor
// flyweights referenced by many line items; and a Null Object standing in
// for a weight-limit handler nobody supplied.
//
// Build and run via scripts/build.sh from the repo root, or standalone from
// this directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>

#include "exception_handler.hpp"
#include "package_tree.hpp"
#include "sku_catalog.hpp"

namespace {

void print_group(const palletizing::PackageComponent& component) {
    std::cout << component.label() << ": " << component.total_weight_grams() << "g, "
              << component.item_count() << " units\n";
}

}  // namespace

int main() {
    using namespace palletizing;

    SkuCatalog catalog;
    catalog.register_sku({"WIDGET-100", "Standard widget", /*unit_weight_grams=*/250, 80, 80, 40, HazmatClass::None});
    catalog.register_sku({"GADGET-200", "Precision gadget", /*unit_weight_grams=*/400, 120, 90, 60, HazmatClass::None});
    catalog.register_sku(
        {"BATTERY-300", "Lithium battery pack", /*unit_weight_grams=*/900, 150, 100, 70, HazmatClass::Lithium});

    std::cout << "== Composite: a mixed tree, one interface ==\n";

    auto carton_a = std::make_shared<PackageGroup>("Carton CTN-A");
    carton_a->add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), /*quantity=*/40, "Austin, TX"));
    carton_a->add(std::make_shared<Item>("line-2", catalog.get("GADGET-200"), /*quantity=*/12, "Austin, TX"));

    auto carton_c = std::make_shared<PackageGroup>("Carton CTN-C");
    carton_c->add(std::make_shared<Item>("line-3", catalog.get("BATTERY-300"), /*quantity=*/8, "Austin, TX"));

    auto carton_b = std::make_shared<PackageGroup>("Carton CTN-B");
    carton_b->add(carton_c);

    auto pallet = std::make_shared<PackageGroup>("Pallet PAL-9");
    pallet->add(carton_a);
    pallet->add(carton_b);

    print_group(*carton_a);
    print_group(*carton_c);
    print_group(*carton_b);
    print_group(*pallet);
    std::cout << "(Pallet's totals are Carton CTN-A's and Carton CTN-B's totals added together,\n"
                 " and Carton CTN-B's total is Carton CTN-C's total, recursed through two levels.)\n";

    std::cout << "\n== Flyweight: a few descriptors, many line items ==\n";
    std::cout << "Distinct SKUs registered in the catalog: 3\n";
    std::cout << "Flyweights materialized so far: " << catalog.pool_size() << '\n';

    // Build 60 more line items across the same 3 SKUs to show the pool stay
    // put: every call below either returns an already-pooled flyweight or
    // materializes one of only 3 possible descriptors.
    auto overflow = std::make_shared<PackageGroup>("Pallet PAL-12");
    for (int i = 0; i < 60; ++i) {
        const char* sku_code = (i % 3 == 0) ? "WIDGET-100" : (i % 3 == 1) ? "GADGET-200" : "BATTERY-300";
        overflow->add(std::make_shared<Item>("overflow-line", catalog.get(sku_code), /*quantity=*/1, "Reno, NV"));
    }
    std::cout << "After adding 60 more line items across the same 3 SKUs:\n";
    std::cout << "Flyweights materialized: " << catalog.pool_size() << " (still 3)\n";

    const auto widget_a = catalog.get("WIDGET-100");
    const auto widget_b = catalog.get("WIDGET-100");
    std::cout << "Two separate get(\"WIDGET-100\") calls return the same instance: "
              << (widget_a.get() == widget_b.get() ? "true" : "false") << '\n';

    std::cout << "\n== Null Object: a weight-limit check with no branch for \"no handler\" ==\n";

    std::cout << "Carton CTN-A (default, unhandled) checked against a 500g limit:\n";
    carton_a->check_weight_limit(500);
    std::cout << "  (no output above: the default NullShipmentExceptionHandler ran and did nothing)\n";

    LoggingShipmentExceptionHandler logger;
    PackageGroup carton_a_logged("Carton CTN-A (logged)", logger);
    carton_a_logged.add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 40, "Austin, TX"));
    carton_a_logged.add(std::make_shared<Item>("line-2", catalog.get("GADGET-200"), 12, "Austin, TX"));
    carton_a_logged.check_weight_limit(500);
    for (const auto& event : logger.events()) {
        std::cout << "  " << event << '\n';
    }

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed error for an unregistered SKU ==\n";
    const auto result = catalog.try_get("CRATE-999");
    if (!result.has_value()) {
        std::cout << "Rejected lookup: " << result.error() << '\n';
    }
#endif

    return 0;
}
