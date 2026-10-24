#include "carrier_contract.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace warehouse {

CarrierContract::CarrierContract(std::string carrier_name, const Cents* tier_rates_cents, std::size_t tier_count)
    : carrier_name_(std::move(carrier_name)), tier_rates_(new Cents[tier_count]), tier_count_(tier_count) {
    std::copy(tier_rates_cents, tier_rates_cents + tier_count, tier_rates_);
}

CarrierContract::CarrierContract(const CarrierContract& other) {
    copy_from(other);
}

CarrierContract& CarrierContract::operator=(const CarrierContract& other) {
    if (this != &other) {
        // Release this object's tier table, then deep-copy the other's.
        // The self-assignment guard above makes the release safe; if the
        // allocation in copy_from throws, this contract is left empty but
        // valid (null pointer, zero size), never half-destroyed.
        release();
        copy_from(other);
    }
    return *this;
}

CarrierContract::CarrierContract(CarrierContract&& other) noexcept
    : carrier_name_(std::move(other.carrier_name_)),
      tier_rates_(std::exchange(other.tier_rates_, nullptr)),
      tier_count_(std::exchange(other.tier_count_, 0)) {
    // No allocation here: the move steals the source's pointer outright
    // and leaves the source empty, which is why move is the cheap
    // operation and copy is the expensive one for a type like this.
}

CarrierContract& CarrierContract::operator=(CarrierContract&& other) noexcept {
    if (this != &other) {
        release();
        carrier_name_ = std::move(other.carrier_name_);
        tier_rates_ = std::exchange(other.tier_rates_, nullptr);
        tier_count_ = std::exchange(other.tier_count_, 0);
    }
    return *this;
}

CarrierContract::~CarrierContract() {
    release();
}

void CarrierContract::copy_from(const CarrierContract& other) {
    carrier_name_ = other.carrier_name_;
    tier_count_ = other.tier_count_;
    tier_rates_ = new Cents[tier_count_];
    std::copy(other.tier_rates_, other.tier_rates_ + tier_count_, tier_rates_);
}

void CarrierContract::release() {
    delete[] tier_rates_;
    tier_rates_ = nullptr;
    tier_count_ = 0;
}

void CarrierContract::set_tier_rate(std::size_t tier, Cents rate_cents) {
    if (tier >= tier_count_) {
        throw std::out_of_range("carrier contract has no such tier");
    }
    tier_rates_[tier] = rate_cents;
}

Cents CarrierContract::tier_rate(std::size_t tier) const {
    if (tier >= tier_count_) {
        throw std::out_of_range("carrier contract has no such tier");
    }
    return tier_rates_[tier];
}

std::size_t CarrierContract::tier_count() const {
    return tier_count_;
}

const std::string& CarrierContract::carrier_name() const {
    return carrier_name_;
}

}  // namespace warehouse
