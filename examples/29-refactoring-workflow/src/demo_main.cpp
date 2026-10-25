#include <chrono>
#include <iomanip>
#include <iostream>

#include "legacy_eta_estimator.hpp"
#include "modern_eta_estimator.hpp"
#include "seamed_eta_estimator.hpp"
#include "shipment_profile.hpp"

namespace {

void print_estimate(const char* label, const refactor29::EtaEstimate& estimate) {
    const auto ymd = estimate.delivery_date;
    std::cout << label << ": " << estimate.transit_days << " day(s), surcharge $"
              << std::fixed << std::setprecision(2) << estimate.surcharge << ", delivery "
              << static_cast<int>(ymd.year()) << '-' << std::setfill('0') << std::setw(2)
              << static_cast<unsigned>(ymd.month()) << '-' << std::setw(2)
              << static_cast<unsigned>(ymd.day()) << std::setfill(' ') << '\n';
}

}  // namespace

int main() {
    using namespace refactor29;

    std::cout << "== Stage 1: Legacy (reads the wall clock inline) ==\n";
    LegacyEtaEstimator legacy;
    print_estimate("Ground, same zone, standard value",
                    legacy.estimate("ground", "A", "A", 10.0, false, 200.0));
    std::cout << "(this line's delivery date depends on whatever day you run it)\n\n";

    const std::chrono::year_month_day july =
        std::chrono::year{2026} / std::chrono::July / 4;
    const std::chrono::year_month_day december =
        std::chrono::year{2026} / std::chrono::December / 15;

    std::cout << "== Stage 2: Seamed (Feathers: Extract Interface + Parameterize Constructor) ==\n";
    const FixedClock july_clock(july);
    const FixedClock december_clock(december);
    const SeamedEtaEstimator seamed_july(july_clock);
    const SeamedEtaEstimator seamed_december(december_clock);
    print_estimate("Ground, same zone, July 4 (no holiday adjustment)",
                    seamed_july.estimate("ground", "A", "A", 10.0, false, 200.0));
    print_estimate("Ground, same zone, December 15 (holiday adjustment)",
                    seamed_december.estimate("ground", "A", "A", 10.0, false, 200.0));
    std::cout << "(now deterministic: same inputs, same clock, same answer every run)\n\n";

    std::cout << "== Stage 3: Modern (Fowler: Extract Function, Replace Conditional with\n"
                 "   Polymorphism, Introduce Parameter Object) ==\n";
    const ModernEtaEstimator modern_july(july_clock);
    const ModernEtaEstimator modern_december(december_clock);
    const ShipmentProfile ground_profile{"ground", "A", "A", 10.0, false, 200.0};
    print_estimate("Ground, same zone, July 4", modern_july.estimate(ground_profile));
    print_estimate("Ground, same zone, December 15", modern_december.estimate(ground_profile));

    const ShipmentProfile freight_profile{"freight", "A", "B", 10.0, false, 200.0};
    print_estimate("Freight, a carrier added after the refactor",
                    modern_july.estimate(freight_profile));
    std::cout << "(identical answers to stage 2 for every pre-existing carrier; freight\n"
                 " required one new class and one new lookup line, nothing else touched)\n";

    return 0;
}
