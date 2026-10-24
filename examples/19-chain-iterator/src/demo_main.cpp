// Chain of Responsibility and Iterator against a shipment-approval domain:
// an approval chain routes a shipment request through weight, hazmat, and
// embargo checks before falling back to manual review, and a
// PackageManifest demonstrates both a hand-written forward iterator and the
// equivalent modern ranges pipeline over the same bucketed storage.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <algorithm>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include "approval_chain.hpp"
#include "package_manifest.hpp"

namespace {

const char* outcome_label(shipping::ApprovalOutcome outcome) {
    switch (outcome) {
        case shipping::ApprovalOutcome::Approved:
            return "Approved";
        case shipping::ApprovalOutcome::Rejected:
            return "Rejected";
        case shipping::ApprovalOutcome::NeedsManualReview:
            return "Needs manual review";
    }
    return "Unknown";
}

void print_decision(const shipping::ShipmentRequest& request, const shipping::ApprovalDecision& decision,
                     const std::vector<std::string>& call_log) {
    std::cout << request.destination_country << " (" << request.weight_kg << " kg"
              << (request.hazmat ? ", hazmat" : "") << "): " << outcome_label(decision.outcome) << " by "
              << decision.handler_name << " -- " << decision.reason << '\n';
    std::cout << "  chain visited: ";
    for (std::size_t i = 0; i < call_log.size(); ++i) {
        std::cout << call_log[i] << (i + 1 < call_log.size() ? " -> " : "");
    }
    std::cout << '\n';
}

}  // namespace

int main() {
    using namespace shipping;

    std::cout << "== Chain of Responsibility: shipment approval ==\n";
    const std::vector<ShipmentRequest> requests{
        {/*weight_kg=*/55.0, /*hazmat=*/false, "US"},
        {/*weight_kg=*/10.0, /*hazmat=*/true, "FR"},
        {/*weight_kg=*/10.0, /*hazmat=*/false, "KP"},
        {/*weight_kg=*/10.0, /*hazmat=*/false, "DE"},
    };
    for (const auto& request : requests) {
        auto chain = build_default_chain();
        std::vector<std::string> call_log;
        const ApprovalDecision decision = chain->handle(request, call_log);
        print_decision(request, decision, call_log);
    }

    std::cout << "\n== Iterator: a hand-written forward iterator over a bucketed manifest ==\n";
    PackageManifest manifest;
    manifest.add(Package{"TRK-INTL-1", 2.0, DestinationZone::International});
    manifest.add(Package{"TRK-LOC-1", 1.0, DestinationZone::Local});
    manifest.add(Package{"TRK-NAT-1", 5.0, DestinationZone::National});
    manifest.add(Package{"TRK-NAT-2", 7.5, DestinationZone::National});
    manifest.add(Package{"TRK-REG-1", 3.0, DestinationZone::Regional});

    std::cout << "Inserted in INTL, LOC, NAT, NAT, REG order; the iterator still visits zone order:\n";
    for (const Package& package : manifest) {
        std::cout << "  " << package.tracking_number << '\n';
    }

    const auto found =
        std::find_if(manifest.begin(), manifest.end(),
                      [](const Package& package) { return package.zone == DestinationZone::Regional; });
    std::cout << "std::find_if locates the first Regional package: " << found->tracking_number << '\n';

    std::cout << "\n== Iterator: the same data through a ranges filter + transform pipeline ==\n";
    auto national_weights =
        manifest | std::views::filter([](const Package& package) { return package.zone == DestinationZone::National; }) |
        std::views::transform([](const Package& package) { return package.weight_kg; });
    std::cout << "National-zone weights (kg): ";
    bool first_weight = true;
    for (double weight : national_weights) {
        if (!first_weight) {
            std::cout << ' ';
        }
        std::cout << weight;
        first_weight = false;
    }
    std::cout << '\n';

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: std::ranges::to collects the pipeline directly ==\n";
    auto national_tracking_numbers =
        manifest | std::views::filter([](const Package& package) { return package.zone == DestinationZone::National; }) |
        std::views::transform([](const Package& package) { return package.tracking_number; }) |
        std::ranges::to<std::vector>();
    std::cout << "National tracking numbers: ";
    bool first_tracking = true;
    for (const auto& tracking_number : national_tracking_numbers) {
        if (!first_tracking) {
            std::cout << ' ';
        }
        std::cout << tracking_number;
        first_tracking = false;
    }
    std::cout << '\n';
#endif

    return 0;
}
