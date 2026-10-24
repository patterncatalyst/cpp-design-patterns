#pragma once

namespace legacy_freight {

// Third-party rate lookup from GlobalFreight's original integration kit.
// This class is the thing an Adapter exists to wrap: this example does not
// own its source and cannot rename its methods or change its units to
// match shipping::RateSource. Weight travels in grams, the destination
// travels as a numeric zone code, and the quote comes back as a plain long
// rather than this book's typed Cents.
class LegacyGlobalFreightRateApi {
public:
    explicit LegacyGlobalFreightRateApi(long surcharge_cents);

    // zone_code: 0 = local, 1 = regional, 2 = national, 3 = international.
    long lookup_rate_cents_for_parcel(long weight_grams, int zone_code) const;

private:
    long surcharge_cents_;
};

}  // namespace legacy_freight
