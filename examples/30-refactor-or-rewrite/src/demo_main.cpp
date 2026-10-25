#include "legacy_pricing_engine.hpp"
#include "modern_pricing_engine.hpp"
#include "strangler_pricing_router.hpp"

#include <iomanip>
#include <iostream>
#include <memory>

namespace {

void print_dollars(const char* label, int cents) {
    std::cout << label << ": $" << std::fixed << std::setprecision(2) << (cents / 100.0) << "\n";
}

}  // namespace

int main() {
    using strangler30::CutoverPolicy;
    using strangler30::LegacyPricingEngine;
    using strangler30::ModernPricingEngine;
    using strangler30::ShipmentRequest;
    using strangler30::StranglerPricingRouter;

    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto modern = std::make_shared<ModernPricingEngine>();
    StranglerPricingRouter router(legacy, modern, CutoverPolicy{});

    const ShipmentRequest acme{"ACME-2019", "NE", "PAC", 20.0, 50'000};
    const ShipmentRequest globex{"GLOBEX", "MW", "MW", 4.0, 15'000};
    const ShipmentRequest widgetco{"WIDGETCO", "SE", "SW", 5.0, 600'000};

    std::cout << "== Release 1: LegacyOnly, nothing has moved yet ==\n";
    router.set_mode(StranglerPricingRouter::Mode::LegacyOnly);
    print_dollars("ACME-2019 quote", router.quote(acme).total_cents);
    print_dollars("GLOBEX quote", router.quote(globex).total_cents);
    print_dollars("WIDGETCO quote", router.quote(widgetco).total_cents);

    std::cout << "\n== Release 2: Compare mode, modern runs silently alongside legacy ==\n";
    router.set_mode(StranglerPricingRouter::Mode::Compare);
    router.quote(acme);
    router.quote(globex);
    router.quote(widgetco);
    std::cout << "disagreements found: " << router.disagreements().size()
              << " (0 means modern is safe to start taking traffic)\n";

    std::cout << "\n== Release 3: Routed, ACME-2019 migrated, GLOBEX and WIDGETCO still legacy ==\n";
    router.set_mode(StranglerPricingRouter::Mode::Routed);
    router.policy().migrate_customer("ACME-2019");
    print_dollars("ACME-2019 quote (now via modern)", router.quote(acme).total_cents);
    print_dollars("GLOBEX quote (still via legacy)", router.quote(globex).total_cents);
    std::cout << "customers migrated so far: " << router.policy().migrated_count() << " of 3\n";

    std::cout << "\n== Release 4: Routed, every customer migrated ==\n";
    router.policy().migrate_customer("GLOBEX");
    router.policy().migrate_customer("WIDGETCO");
    print_dollars("GLOBEX quote (now via modern)", router.quote(globex).total_cents);
    print_dollars("WIDGETCO quote (now via modern)", router.quote(widgetco).total_cents);
    std::cout << "customers migrated so far: " << router.policy().migrated_count() << " of 3\n";

    std::cout << "\n== Release 5: ModernOnly, LegacyPricingEngine can be deleted ==\n";
    router.set_mode(StranglerPricingRouter::Mode::ModernOnly);
    print_dollars("ACME-2019 quote", router.quote(acme).total_cents);
    print_dollars("GLOBEX quote", router.quote(globex).total_cents);
    print_dollars("WIDGETCO quote", router.quote(widgetco).total_cents);

    return 0;
}
