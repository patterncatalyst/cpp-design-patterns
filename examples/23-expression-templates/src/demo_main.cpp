// Expression templates (lead idiom) plus two supporting idioms: a
// self-registering Prototype registry and templated field-driven
// serialization, all drawn from a multi-leg freight route reporting
// domain. See README.md "Build, run, observe" for the full command set.

#include <iostream>

#include "carrier_policy.hpp"
#include "carrier_profile_serialization.hpp"
#include "construction_tracking.hpp"
#include "eager_route_metrics.hpp"
#include "route_metrics.hpp"

namespace {

void print_metrics(const char* label, double distance_km, double fuel_liters, double dwell_hours) {
    std::cout << label << ": distance=" << distance_km << "km fuel=" << fuel_liters << "L dwell=" << dwell_hours
              << "h\n";
}

}  // namespace

int main() {
    using namespace exprtmpl;

    std::cout << "== Expression templates: weighted multi-leg route summary ==\n";
    const RouteMetrics leg_one(120.0, 18.5, 0.75);
    const RouteMetrics leg_two(64.0, 9.0, 1.25);
    const RouteMetrics leg_three(210.0, 31.0, 0.5);

    reset_construction_count();
    const RouteMetrics lazy_total = 0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three;
    const std::size_t lazy_constructions = construction_count();
    print_metrics("Lazy weighted total", lazy_total.distance_km(), lazy_total.fuel_liters(), lazy_total.dwell_hours());
    std::cout << "RouteMetrics objects constructed for the lazy expression: " << lazy_constructions << '\n';

    const NaiveRouteMetrics naive_one(120.0, 18.5, 0.75);
    const NaiveRouteMetrics naive_two(64.0, 9.0, 1.25);
    const NaiveRouteMetrics naive_three(210.0, 31.0, 0.5);

    reset_construction_count();
    const NaiveRouteMetrics eager_total = 0.5 * naive_one + 0.3 * naive_two + 0.2 * naive_three;
    const std::size_t eager_constructions = construction_count();
    print_metrics("Eager weighted total", eager_total.distance_km(), eager_total.fuel_liters(),
                  eager_total.dwell_hours());
    std::cout << "NaiveRouteMetrics objects constructed for the eager expression: " << eager_constructions << '\n';

    std::cout << "\n== Self-registering prototype registry: carrier policies ==\n";
    auto& registry = CarrierPolicyRegistry::instance();
    for (const char* key : {"flat-rate", "weight-tiered", "regional-express"}) {
        const auto policy = registry.create(key);
        std::cout << policy->name() << ": base rate multiplier x" << policy->base_rate_multiplier() << '\n';
    }
    std::cout << registry.size() << " carrier policies registered, none of them listed by hand in this file.\n";

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed lookup failure via std::expected ==\n";
    if (const auto result = registry.try_create(""); !result.has_value()) {
        std::cout << "Rejected: " << result.error() << '\n';
    }
#endif

    std::cout << "\n== Templated serialization: round-tripping a carrier profile ==\n";
    const CarrierProfile profile{"Northline Freight", 1.35, 18000.0};
    const std::string serialized = serialize(profile);
    std::cout << serialized;
    const CarrierProfile round_tripped = deserialize(serialized);
    std::cout << "Round trip reproduced an equal profile: " << (profile == round_tripped ? "yes" : "no") << '\n';

    return 0;
}
