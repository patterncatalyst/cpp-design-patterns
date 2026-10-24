#include "invoice_rules.hpp"

#include <algorithm>
#include <cmath>

namespace billing {

namespace {
double clamp_rate(double rate) {
    return std::clamp(rate, 0.0, 1.0);
}
}  // namespace

PercentageTaxRule::PercentageTaxRule(double rate) : rate_(clamp_rate(rate)) {}

Cents PercentageTaxRule::adjustment_for(Cents subtotal) const {
    return static_cast<Cents>(std::llround(static_cast<double>(subtotal) * rate_));
}

std::string_view PercentageTaxRule::name() const {
    return "sales tax";
}

PercentageDiscountRule::PercentageDiscountRule(double rate) : rate_(clamp_rate(rate)) {}

Cents PercentageDiscountRule::adjustment_for(Cents subtotal) const {
    const double clamped_rate = clamp_rate(discount_rate(subtotal));
    return -static_cast<Cents>(std::llround(static_cast<double>(subtotal) * clamped_rate));
}

std::string_view PercentageDiscountRule::name() const {
    return "percentage discount";
}

double PercentageDiscountRule::discount_rate(Cents /*subtotal*/) const {
    return rate_;
}

LoyaltyDiscountRule::LoyaltyDiscountRule(double base_rate, int loyalty_years)
    : PercentageDiscountRule(base_rate), bonus_rate_(0.01 * static_cast<double>(std::min(loyalty_years, 10))) {}

std::string_view LoyaltyDiscountRule::name() const {
    return "loyalty discount";
}

double LoyaltyDiscountRule::discount_rate(Cents subtotal) const {
    return PercentageDiscountRule::discount_rate(subtotal) + bonus_rate_;
}

FlatServiceFeeRule::FlatServiceFeeRule(Cents flat_cents) : flat_cents_(flat_cents) {}

Cents FlatServiceFeeRule::adjustment_for(Cents /*subtotal*/) const {
    return flat_cents_;
}

std::string_view FlatServiceFeeRule::name() const {
    return "service fee";
}

}  // namespace billing
