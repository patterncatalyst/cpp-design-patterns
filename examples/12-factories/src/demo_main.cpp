// Carrier integration, shown two idiomatic C++ ways: an Abstract Factory
// producing a matching family of three products per carrier, and a
// Factory Method deciding which single integration test suite to run per
// environment. Build and run via scripts/build.sh from the repo root, or
// standalone from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>

#include "abstract_factory.hpp"
#include "factory_method.hpp"
#include "shipping_domain.hpp"

namespace {

void print_processed(const char* carrier_label, const carrier_integration::ProcessedShipment& processed) {
    std::cout << carrier_label << ":\n"
              << "  label:   " << processed.label << '\n'
              << "  rate:    " << processed.rate_cents << " cents\n"
              << "  tracking: " << processed.tracking_url << '\n';
}

void print_onboarding(const char* label, const carrier_integration::OnboardingResult& result) {
    std::cout << label << " (" << (result.passed ? "PASSED" : "FAILED") << "):\n";
    for (const std::string& line : result.log) {
        std::cout << "  " << line << '\n';
    }
}

}  // namespace

int main() {
    using namespace carrier_integration;

    const Shipment shipment{/*destination_country=*/"CA", /*weight_kg=*/4.2, /*declared_value_cents=*/18000};

    std::cout << "== Abstract Factory: two carrier families, one client ==\n";
    const FastShipIntegrationFactory fastship_factory("acct-fastship-77");
    const ShipmentProcessor fastship_processor(fastship_factory);
    print_processed("FastShip", fastship_processor.process(shipment, "TRK-1001"));

    const SummitFreightIntegrationFactory summit_factory("acct-summit-42");
    const ShipmentProcessor summit_processor(summit_factory);
    print_processed("SummitFreight", summit_processor.process(shipment, "TRK-2002"));

    std::cout << "\n== Abstract Factory: carrier chosen by name at startup ==\n";
    const auto looked_up_factory = make_carrier_factory("fastship", "acct-fastship-77");
    const ShipmentProcessor looked_up_processor(*looked_up_factory);
    print_processed("Looked up (\"fastship\")", looked_up_processor.process(shipment, "TRK-3003"));

    std::cout << "\n== Factory Method: environment decides the test suite ==\n";
    const CarrierAccount sandbox_account{"fastship", "acct-fastship-77", /*webhook_registered=*/false};
    const SandboxOnboarder sandbox_onboarder;
    print_onboarding("Sandbox onboarding", sandbox_onboarder.onboard(sandbox_account));

    const CarrierAccount production_account{"fastship", "acct-fastship-77", /*webhook_registered=*/true};
    const ProductionOnboarder production_onboarder;
    print_onboarding("Production onboarding", production_onboarder.onboard(production_account));

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed error for an unknown carrier ==\n";
    const auto result = try_make_carrier_factory("globex-shipping", "acct-unknown-01");
    if (!result.has_value()) {
        std::cout << "Rejected unknown carrier: " << result.error() << '\n';
    }
#endif

    return 0;
}
