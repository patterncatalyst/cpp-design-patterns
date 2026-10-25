#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "legacy_carrier_zone_lookup.hpp"
#include "legacy_fuel_surcharge.hpp"
#include "manifest_source.hpp"
#include "shipment_alert_notifier.hpp"
#include "shipment_eta_scheduler.hpp"

namespace {

class RecordingAlertChannel : public breakdep28::AlertChannel {
public:
    void send(const std::string& recipient, const std::string& message) const override {
        std::cout << "  recorded (no real SMTP call): to=" << recipient << " \"" << message << "\"\n";
    }
};

}  // namespace

int main() {
    using namespace breakdep28;

    std::cout << "== Extract Interface + Parameterize Constructor: ShipmentAlertNotifier ==\n";
    ShipmentAlertNotifier production_notifier("ops-supervisor@example.com");
    production_notifier.flag_for_manual_review("SH-5001", "weight exceeds declared value");

    auto recording_channel = std::make_shared<RecordingAlertChannel>();
    ShipmentAlertNotifier test_style_notifier("ops-supervisor@example.com", recording_channel);
    test_style_notifier.flag_for_manual_review("SH-5002", "recipient address flagged");

    std::cout << "\n== Extract and Override Call: ShipmentEtaScheduler ==\n";
    ShipmentEtaScheduler production_scheduler(4);
    const auto live_eta = production_scheduler.schedule_shipment("SH-6001");
    (void)live_eta;
    std::cout << "  live schedule for SH-6001 computed against the real clock\n";

#if __cplusplus >= 202302L
    DeducingEtaScheduler deducing_scheduler(4);
    const auto deducing_eta = deducing_scheduler.schedule_shipment("SH-6002");
    (void)deducing_eta;
    std::cout << "  deducing-this variant (C++23) computed the same shape, no vtable involved\n";
#endif

    std::cout << "\n== Encapsulate Global References: ShipmentCostCalculator ==\n";
    legacy_global::set_fuel_surcharge(1.18);
    LiveFuelSurchargeSource live_source;
    ShipmentCostCalculator live_calculator(live_source);
    std::cout << "  live total for a $100.00 base at the legacy global's current rate: $" << std::fixed
              << std::setprecision(2) << live_calculator.total_cost(100.0) << '\n';

    std::cout << "\n== Introduce Instance Delegator: ShipmentRoutingAdvisor ==\n";
    CarrierZoneLookupDelegator delegator;
    ShipmentRoutingAdvisor advisor(delegator);
    std::cout << "  " << advisor.route_hint("02108") << '\n';
    std::cout << "  legacy call site unchanged: " << ZoneSurchargeReport::describe("99501") << '\n';

    std::cout << "\n== Adapt Parameter: WarehouseDispatcher ==\n";
    const auto temp_path = std::filesystem::temp_directory_path() / "breakdep28_demo_manifest.txt";
    {
        std::ofstream out(temp_path);
        out << "FASTFREIGHT\n275\n";
    }
    const CarrierManifestFile manifest_file(temp_path.string());
    const CarrierManifestFileAdapter adapter(manifest_file);
    WarehouseDispatcher dispatcher(200);
    const auto summary = dispatcher.dispatch(adapter);
    std::cout << "  dispatched " << summary.shipment_count << " shipments for " << summary.carrier_code
              << (summary.large_batch ? " (large batch)" : "") << '\n';
    std::filesystem::remove(temp_path);

    return 0;
}
