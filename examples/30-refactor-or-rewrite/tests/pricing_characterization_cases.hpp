#pragma once

#include "pricing_engine.hpp"

#include <vector>

namespace strangler30 {

struct CharacterizationCase {
    const char* name;
    ShipmentRequest request;
    int expected_total_cents;
};

// Captured from LegacyPricingEngine's observed behavior before any rewrite
// work started. This table is what a characterization test means in
// practice: nobody derived these totals from a specification, they were
// read off the running legacy engine and pinned here as the oracle both
// LegacyPricingEngine and ModernPricingEngine are judged against.
inline const std::vector<CharacterizationCase>& pricing_characterization_cases() {
    static const std::vector<CharacterizationCase> cases{
        {"same_zone_no_frills",
         ShipmentRequest{"WIDGETCO", "NE", "NE", 2.0, 10'000},
         500},
        {"cross_country_lane",
         ShipmentRequest{"WIDGETCO", "NE", "PAC", 10.0, 50'000},
         1650},
        {"high_value_insurance_surcharge",
         ShipmentRequest{"WIDGETCO", "SE", "SW", 5.0, 600'000},
         6875},
        {"acme_volume_rebate_does_not_apply_below_threshold",
         ShipmentRequest{"ACME-2019", "MW", "SE", 8.0, 20'000},
         975},
        {"acme_volume_rebate_applies_above_threshold",
         ShipmentRequest{"ACME-2019", "NE", "PAC", 20.0, 50'000},
         1600},
        {"globex_flat_contract_discount",
         ShipmentRequest{"GLOBEX", "MW", "MW", 4.0, 15'000},
         500},
        {"fractional_weight_rounds_up_not_down",
         ShipmentRequest{"WIDGETCO", "SW", "SW", 0.333, 1'000},
         325},
    };
    return cases;
}

}  // namespace strangler30
