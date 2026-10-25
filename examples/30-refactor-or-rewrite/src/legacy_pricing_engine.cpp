#include "legacy_pricing_engine.hpp"

#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace strangler30 {

namespace {

// Zone-to-zone base rate, in cents, between the carrier's five regional
// hubs (Northeast, Midwest, Southeast, Southwest, Pacific). The values are
// not a formula; they are what the rate desk actually quotes, adjusted by
// hand across several years of lane negotiations. A lane prices the same
// in both directions, so the table only stores one entry per unordered
// pair.
const std::map<std::pair<std::string, std::string>, int>& zone_matrix() {
    static const std::map<std::pair<std::string, std::string>, int> table{
        {{"NE", "NE"}, 400},  {{"MW", "MW"}, 350},  {{"SE", "SE"}, 300},
        {{"SW", "SW"}, 300},  {{"PAC", "PAC"}, 300}, {{"MW", "NE"}, 650},
        {{"NE", "SE"}, 700},  {{"NE", "SW"}, 950},  {{"NE", "PAC"}, 1200},
        {{"MW", "SE"}, 600},  {{"MW", "SW"}, 700},  {{"MW", "PAC"}, 1000},
        {{"SE", "SW"}, 650},  {{"SE", "PAC"}, 1100}, {{"SW", "PAC"}, 500},
    };
    return table;
}

std::pair<std::string, std::string> normalized_pair(const std::string& a, const std::string& b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

int round_up_cents(double raw_cents) {
    return static_cast<int>(std::ceil(raw_cents - 1e-9));
}

// A step inherited from a decommissioned mainframe invoicing system. No
// one still at the company can say whether the downstream system that
// consumed quarter-rounded totals is still in service, so every
// characterization case pins this rule rather than letting a rewrite drop
// it by accident.
int round_up_to_quarter_dollar(int cents) {
    const int quarter = 25;
    return ((cents + quarter - 1) / quarter) * quarter;
}

}  // namespace

Quote LegacyPricingEngine::quote(const ShipmentRequest& request) const {
    if (request.weight_kg < 0.0) {
        throw std::invalid_argument("shipment weight_kg cannot be negative");
    }

    const int base = zone_base_cents(request.origin_zone, request.destination_zone);
    const int weight_component = round_up_cents(request.weight_kg * 45.0);
    int subtotal = base + weight_component;

    // Declared-value insurance surcharge: 1% on anything over $5,000. This
    // part of the rate card is written down in the carrier's published
    // tariff, unlike everything that follows it.
    if (request.declared_value_cents > 500'000) {
        subtotal += request.declared_value_cents / 100;
    }

    subtotal = apply_customer_overrides(request.customer_id, subtotal);

    return Quote{round_up_to_quarter_dollar(subtotal),
                 "legacy zone matrix + customer override + quarter rounding"};
}

int LegacyPricingEngine::zone_base_cents(const std::string& origin, const std::string& destination) const {
    const auto key = normalized_pair(origin, destination);
    const auto& table = zone_matrix();
    const auto it = table.find(key);
    if (it == table.end()) {
        throw std::invalid_argument("unrecognized zone pair: " + origin + "/" + destination);
    }
    return it->second;
}

int LegacyPricingEngine::apply_customer_overrides(const std::string& customer_id, int subtotal_cents) const {
    // Two negotiated contracts, each expressed as a one-off branch rather
    // than a shared discount mechanism. Nothing marks either branch as a
    // contract term; a reader has to already know ACME-2019's volume
    // rebate exists to recognize this code as implementing it.
    if (customer_id == "ACME-2019" && subtotal_cents > 2000) {
        return subtotal_cents - 500;
    }
    if (customer_id == "GLOBEX") {
        return static_cast<int>(subtotal_cents * 0.90);
    }
    return subtotal_cents;
}

}  // namespace strangler30
