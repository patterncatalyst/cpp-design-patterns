#include "rate_decorators.hpp"

#include <cmath>
#include <utility>

namespace shipping {

namespace {

// A cache key built from the three fields that drive a quote. Two
// Shipment values with the same weight, zone, and declared value always
// price identically, so they share a cache entry.
std::string cache_key(const Shipment& shipment) {
    return std::to_string(shipment.weight_kg) + "|" + std::to_string(static_cast<int>(shipment.zone)) + "|" +
           std::to_string(shipment.declared_value_cents);
}

}  // namespace

Cents TariffTableRateSource::quote(const Shipment& shipment) {
    return base_tariff_cost(shipment);
}

LoggingRateSource::LoggingRateSource(std::unique_ptr<RateSource> inner, std::vector<std::string>& call_log)
    : inner_(std::move(inner)), call_log_(call_log) {}

Cents LoggingRateSource::quote(const Shipment& shipment) {
    call_log_.push_back("quote requested for a " + std::to_string(shipment.weight_kg) + "kg shipment");
    return inner_->quote(shipment);
}

CachingRateSource::CachingRateSource(std::unique_ptr<RateSource> inner) : inner_(std::move(inner)) {}

Cents CachingRateSource::quote(const Shipment& shipment) {
    const std::string key = cache_key(shipment);
    const auto found = cache_.find(key);
    if (found != cache_.end()) {
        ++cache_hits_;
        return found->second;
    }

    ++cache_misses_;
    const Cents price = inner_->quote(shipment);
    cache_.emplace(key, price);
    return price;
}

SurchargeRateSource::SurchargeRateSource(std::unique_ptr<RateSource> inner, double cents_per_declared_dollar)
    : inner_(std::move(inner)), cents_per_declared_dollar_(cents_per_declared_dollar) {}

Cents SurchargeRateSource::quote(const Shipment& shipment) {
    const Cents base = inner_->quote(shipment);
    const double declared_dollars = static_cast<double>(shipment.declared_value_cents) / 100.0;
    const auto surcharge = static_cast<Cents>(std::ceil(declared_dollars * cents_per_declared_dollar_));
    return base + surcharge;
}

void SurchargeRateSource::set_cents_per_declared_dollar(double cents_per_declared_dollar) {
    cents_per_declared_dollar_ = cents_per_declared_dollar;
}

}  // namespace shipping
