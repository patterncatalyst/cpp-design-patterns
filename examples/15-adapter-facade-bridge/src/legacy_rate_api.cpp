#include "legacy_rate_api.hpp"

namespace legacy_freight {

LegacyGlobalFreightRateApi::LegacyGlobalFreightRateApi(long surcharge_cents) : surcharge_cents_(surcharge_cents) {}

long LegacyGlobalFreightRateApi::lookup_rate_cents_for_parcel(long weight_grams, int zone_code) const {
    // GlobalFreight's own rate table: one cent per ten grams, plus a flat
    // 200-cent step per zone code, plus whatever surcharge this account was
    // configured with. The math itself is not the point of this example;
    // the point is that this signature and these units are fixed, and the
    // adapter has to translate to and from them on every call.
    const long grams_component = weight_grams / 10;
    const long zone_component = static_cast<long>(zone_code) * 200;
    return grams_component + zone_component + surcharge_cents_;
}

}  // namespace legacy_freight
