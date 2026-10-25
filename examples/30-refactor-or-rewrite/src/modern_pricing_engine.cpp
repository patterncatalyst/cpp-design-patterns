#include "modern_pricing_engine.hpp"

#include <cmath>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace strangler30 {

namespace {

// Reverse engineered from LegacyPricingEngine's zone matrix. These are not
// a formula here either; they are the same hand-negotiated lane rates,
// carried forward because the rewrite's job is to preserve this knowledge,
// not to improve on it.
const std::map<std::pair<std::string, std::string>, int>& zone_rate_card() {
    static const std::map<std::pair<std::string, std::string>, int> table{
        {{"NE", "NE"}, 400},  {{"MW", "MW"}, 350},  {{"SE", "SE"}, 300},
        {{"SW", "SW"}, 300},  {{"PAC", "PAC"}, 300}, {{"MW", "NE"}, 650},
        {{"NE", "SE"}, 700},  {{"NE", "SW"}, 950},  {{"NE", "PAC"}, 1200},
        {{"MW", "SE"}, 600},  {{"MW", "SW"}, 700},  {{"MW", "PAC"}, 1000},
        {{"SE", "SW"}, 650},  {{"SE", "PAC"}, 1100}, {{"SW", "PAC"}, 500},
    };
    return table;
}

const std::unordered_map<std::string, CustomerAdjustment>& customer_adjustments() {
    static const std::unordered_map<std::string, CustomerAdjustment> table{
        {"ACME-2019",
         CustomerAdjustment{CustomerAdjustment::Kind::FlatCentsOffAboveThreshold, 500.0, 2000}},
        {"GLOBEX", CustomerAdjustment{CustomerAdjustment::Kind::PercentOff, 0.10, 0}},
    };
    return table;
}

std::pair<std::string, std::string> normalized_pair(const std::string& a, const std::string& b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

int round_up_cents(double raw_cents) {
    return static_cast<int>(std::ceil(raw_cents - 1e-9));
}

int round_up_to_quarter_dollar(int cents) {
    const int quarter = 25;
    return ((cents + quarter - 1) / quarter) * quarter;
}

}  // namespace

Quote ModernPricingEngine::quote(const ShipmentRequest& request) const {
    if (request.weight_kg < 0.0) {
        throw std::invalid_argument("shipment weight_kg cannot be negative");
    }

    const int base = zone_base_cents(request.origin_zone, request.destination_zone);
    const int weight_component = round_up_cents(request.weight_kg * 45.0);
    int subtotal = base + weight_component;

    if (request.declared_value_cents > 500'000) {
        subtotal += request.declared_value_cents / 100;
    }

    subtotal = apply_customer_adjustment(request.customer_id, subtotal);

    return Quote{round_up_to_quarter_dollar(subtotal),
                 "modern zone rate card + named adjustment + preserved rounding"};
}

int ModernPricingEngine::zone_base_cents(const std::string& origin, const std::string& destination) const {
    const auto key = normalized_pair(origin, destination);
    const auto& table = zone_rate_card();
    const auto it = table.find(key);
    if (it == table.end()) {
        throw std::invalid_argument("unrecognized zone pair: " + origin + "/" + destination);
    }
    return it->second;
}

int ModernPricingEngine::apply_customer_adjustment(const std::string& customer_id, int subtotal_cents) const {
    const auto& table = customer_adjustments();
    const auto it = table.find(customer_id);
    if (it == table.end()) {
        return subtotal_cents;
    }

    const CustomerAdjustment& adjustment = it->second;
    if (adjustment.kind == CustomerAdjustment::Kind::FlatCentsOffAboveThreshold) {
        if (subtotal_cents > adjustment.threshold_cents) {
            return subtotal_cents - static_cast<int>(adjustment.value);
        }
        return subtotal_cents;
    }

    return static_cast<int>(subtotal_cents * (1.0 - adjustment.value));
}

}  // namespace strangler30
