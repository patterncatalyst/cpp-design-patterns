#include <chrono>
#include <iomanip>
#include <iostream>

#include "clock.hpp"
#include "late_fee_schedule.hpp"
#include "legacy_shipment_sla_checker.hpp"
#include "shipment_sla.hpp"
#include "shipment_sla_checker.hpp"

namespace {

void print_breach(const char* label, const seam27::ShipmentSla& sla, bool breached) {
    std::cout << label << ' ' << sla.tracking_id << " breached: " << std::boolalpha << breached << '\n';
}

}  // namespace

int main() {
    using namespace seam27;
    using namespace std::chrono;

    std::cout << "== Legacy checker: hard dependency on the wall clock, no seam ==\n";
    LegacyShipmentSlaChecker legacy;
    const ShipmentSla overdue{"SHM-7001", system_clock::now() - hours(2)};
    const ShipmentSla pending{"SHM-7002", system_clock::now() + hours(2)};
    print_breach("legacy", overdue, legacy.is_breached(overdue));
    print_breach("legacy", pending, legacy.is_breached(pending));

    std::cout << "\n== Seamed checker: Clock injected through the constructor ==\n";
    SystemClock real_clock;
    ShipmentSlaChecker checker(real_clock);
    print_breach("seamed", overdue, checker.is_breached(overdue));
    print_breach("seamed", pending, checker.is_breached(pending));

    std::cout << "\n== Late fee schedule: grace period reached through the link seam ==\n";
    std::cout << "5 minutes late: " << late_fee_cents(5) << " cents"
              << " (within production's 15-minute grace)\n";
    std::cout << "25 minutes late: " << late_fee_cents(25) << " cents\n";

    return 0;
}
