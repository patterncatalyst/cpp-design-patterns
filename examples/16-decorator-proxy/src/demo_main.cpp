// Decorator and Proxy against a shared shipping domain: a decorator stack
// prices a Shipment through surcharge, caching, and logging layers wrapped
// around a plain tariff lookup, and a pair of proxies stand in for an
// expensive manifest archive, one deferring its construction and one
// gating access to it. Every layer in both halves of this example
// implements the same interface its wrapped object implements.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>
#include <vector>

#include "manifest_proxy.hpp"
#include "rate_decorators.hpp"
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

    const Shipment cart_item{/*weight_kg=*/2.0, DestinationZone::Regional, /*declared_value_cents=*/10000};

    std::cout << "== Decorator: surcharge wraps caching wraps logging wraps core ==\n";
    {
        std::vector<std::string> call_log;
        auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
        auto caching_owned = std::make_unique<CachingRateSource>(std::move(logging));
        CachingRateSource* caching = caching_owned.get();
        auto surcharge = std::make_unique<SurchargeRateSource>(std::move(caching_owned),
                                                                 /*cents_per_declared_dollar=*/1.5);

        print_quote("First quote (rate 1.5 cents/declared dollar)", surcharge->quote(cart_item));
        surcharge->set_cents_per_declared_dollar(3.0);
        print_quote("Second quote, rate raised to 3.0 (base still cached)", surcharge->quote(cart_item));

        std::cout << "Cache misses: " << caching->cache_misses() << ", cache hits: " << caching->cache_hits()
                   << ", log lines: " << call_log.size() << '\n';
        std::cout << "The surcharge recomputes fresh on every call, so raising the rate changes the\n"
                      "total even though the base rate underneath it only computed once.\n";
    }

    std::cout << "\n== Decorator: reordering it to caching wraps surcharge wraps logging wraps core ==\n";
    {
        std::vector<std::string> call_log;
        auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
        auto surcharge_owned =
            std::make_unique<SurchargeRateSource>(std::move(logging), /*cents_per_declared_dollar=*/1.5);
        SurchargeRateSource* surcharge = surcharge_owned.get();
        auto caching = std::make_unique<CachingRateSource>(std::move(surcharge_owned));

        print_quote("First quote (rate 1.5 cents/declared dollar)", caching->quote(cart_item));
        surcharge->set_cents_per_declared_dollar(3.0);
        print_quote("Second quote, rate raised to 3.0 (total now cached)", caching->quote(cart_item));

        std::cout << "Cache misses: " << caching->cache_misses() << ", cache hits: " << caching->cache_hits()
                   << ", log lines: " << call_log.size() << '\n';
        std::cout << "Caching now sits outside the surcharge, so the already-surcharged total is\n"
                      "what gets memoized; raising the rate has no effect until the shipment key changes.\n";
    }

    std::cout << "\n== Proxy: lazy initialization defers the expensive manifest archive ==\n";
    {
        LazyManifestStoreProxy lazy_store;
        std::cout << "Archive loaded before first lookup? " << (lazy_store.is_loaded() ? "yes" : "no") << '\n';
        std::cout << "Lookup TRK-1001: " << lazy_store.lookup("TRK-1001") << '\n';
        std::cout << "Archive loaded after first lookup?  " << (lazy_store.is_loaded() ? "yes" : "no") << '\n';
        std::cout << "Lookup TRK-2002: " << lazy_store.lookup("TRK-2002") << '\n';
    }

    std::cout << "\n== Proxy: access control gates the same interface, stacked on the lazy proxy ==\n";
    {
        AccessControlledManifestProxy guest_view(std::make_unique<LazyManifestStoreProxy>(), AccessLevel::Guest);
        AccessControlledManifestProxy warehouse_view(std::make_unique<LazyManifestStoreProxy>(),
                                                       AccessLevel::Warehouse);

        try {
            guest_view.lookup("TRK-1001");
        } catch (const std::runtime_error& denied) {
            std::cout << "Guest lookup rejected: " << denied.what() << '\n';
        }
        std::cout << "Warehouse lookup: " << warehouse_view.lookup("TRK-1001") << '\n';
    }

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed access-denial error via std::expected ==\n";
    AccessControlledManifestProxy guest_view(std::make_unique<LazyManifestStoreProxy>(), AccessLevel::Guest);
    if (const auto result = guest_view.try_lookup("TRK-1001"); !result.has_value()) {
        std::cout << "Rejected guest lookup: " << result.error() << '\n';
    }
#endif

    return 0;
}
