#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace warehouse {

using Cents = std::int64_t;

// A negotiated carrier contract: a per-tier rate table the carrier
// publishes once and every route under that contract quotes against.
// CarrierContract owns a raw, heap-allocated rate table directly (a
// Cents*, not a std::vector<Cents>) specifically so this example has a
// real resource to manage by hand and a real Rule of Five to write. Every
// other type in this chapter needs none of this; a type only needs a
// destructor, a copy constructor, a copy assignment operator, a move
// constructor, and a move assignment operator when it owns a resource the
// compiler cannot already copy or move correctly on its own.
class CarrierContract {
public:
    CarrierContract(std::string carrier_name, const Cents* tier_rates_cents, std::size_t tier_count);

    // Rule of Five, all five declared because this type owns a raw
    // resource (tier_rates_) that none of the compiler-generated special
    // members could manage safely on their own.
    CarrierContract(const CarrierContract& other);
    CarrierContract& operator=(const CarrierContract& other);
    CarrierContract(CarrierContract&& other) noexcept;
    CarrierContract& operator=(CarrierContract&& other) noexcept;
    ~CarrierContract();

    void set_tier_rate(std::size_t tier, Cents rate_cents);
    Cents tier_rate(std::size_t tier) const;
    std::size_t tier_count() const;
    const std::string& carrier_name() const;

private:
    void copy_from(const CarrierContract& other);
    void release();

    std::string carrier_name_;
    Cents* tier_rates_ = nullptr;
    std::size_t tier_count_ = 0;
};

}  // namespace warehouse
