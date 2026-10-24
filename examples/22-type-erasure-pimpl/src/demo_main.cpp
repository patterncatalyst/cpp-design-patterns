// Type erasure and PImpl, taught together in one checkout-quoting flow.
//
// AnyRateQuoter erases several unrelated concrete quoter types behind one
// value-semantic wrapper, with no shared base class. RateQuoteLedger
// records every quote behind a PImpl compilation firewall and still
// copies deeply and moves correctly.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <vector>

#include "any_rate_quoter.hpp"
#include "rate_quote_ledger.hpp"
#include "rate_quoters.hpp"
#include "shipping_domain.hpp"

namespace {

void print_quote(const char* label, shipping::Cents cents) {
    const auto dollars = cents / 100;
    const auto remainder = cents % 100;
    std::cout << label << ": $" << dollars << "." << (remainder < 10 ? "0" : "") << remainder << '\n';
}

}  // namespace

int main() {
    using namespace shipping;

    const Shipment cart_item{/*weight_kg=*/3.5, DestinationZone::Regional, /*declared_value_cents=*/12000};

    std::cout << "== Type erasure: unrelated concrete quoters in one container ==\n";
    std::vector<AnyRateQuoter> quoters;
    quoters.emplace_back(FlatRateQuoter{999});
    quoters.emplace_back(WeightBasedQuoter{500, 150});
    quoters.emplace_back(InsuredQuoter(AnyRateQuoter(WeightBasedQuoter{500, 150}), /*cents_per_declared_dollar=*/1.5));

    print_quote("Flat-rate quoter", quoters[0].quote(cart_item));
    print_quote("Weight-based quoter", quoters[1].quote(cart_item));
    print_quote("Insured weight-based quoter (wraps another AnyRateQuoter)", quoters[2].quote(cart_item));

    std::cout << "\n== Type erasure: a copy is independent of the original's mutable state ==\n";
    AnyRateQuoter surge_original(SurgeQuoter{1000, 25});
    print_quote("Surge quoter, call 1 (original)", surge_original.quote(cart_item));
    AnyRateQuoter surge_copy = surge_original;
    print_quote("Surge quoter, call 2 (copy)", surge_copy.quote(cart_item));
    print_quote("Surge quoter, call 3 (copy again)", surge_copy.quote(cart_item));
    print_quote("Surge quoter, call 2 (original, untouched by the copy)", surge_original.quote(cart_item));

    std::cout << "\n== PImpl: RateQuoteLedger copies deeply, not just the pointer ==\n";
    RateQuoteLedger ledger;
    ledger.record("flat-rate quoter", quoters[0].quote(cart_item));
    ledger.record("weight-based quoter", quoters[1].quote(cart_item));
    ledger.record("insured weight-based quoter", quoters[2].quote(cart_item));
    std::cout << ledger.summary() << '\n';

    RateQuoteLedger ledger_copy = ledger;
    ledger_copy.record("rebate adjustment", 50);
    std::cout << "original ledger entry count after copying: " << ledger.entry_count() << '\n';
    std::cout << "copy's ledger entry count after one more record(): " << ledger_copy.entry_count() << '\n';

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed rejection via std::expected ==\n";
    if (const auto result = ledger.try_record("refund", -100); !result.has_value()) {
        std::cout << "Rejected malformed quote: " << result.error() << '\n';
    }
#endif

    return 0;
}
