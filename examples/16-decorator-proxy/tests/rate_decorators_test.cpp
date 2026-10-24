#include "rate_decorators.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace shipping {
namespace {

TEST(TariffTableRateSourceTest, QuotesTheBaseRateWithNoDecoration) {
    TariffTableRateSource core;
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    // Zone base 700 + ceil(2.0kg * 120 cents/kg) = 700 + 240 = 940.
    EXPECT_EQ(core.quote(shipment), 940);
}

TEST(LoggingRateSourceTest, RecordsOneLineAndForwardsTheQuoteUnchanged) {
    std::vector<std::string> call_log;
    LoggingRateSource logging(std::make_unique<TariffTableRateSource>(), call_log);
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    EXPECT_EQ(logging.quote(shipment), 940);
    ASSERT_EQ(call_log.size(), 1u);
    EXPECT_EQ(call_log.front(), "quote requested for a 2.000000kg shipment");
}

TEST(CachingRateSourceTest, ReturnsTheSameQuoteOnARepeatLookupWithoutRecomputing) {
    std::vector<std::string> call_log;
    auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
    CachingRateSource caching(std::move(logging));
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    EXPECT_EQ(caching.quote(shipment), 940);
    EXPECT_EQ(caching.quote(shipment), 940);

    EXPECT_EQ(caching.cache_misses(), 1u);
    EXPECT_EQ(caching.cache_hits(), 1u);
    // The inner LoggingRateSource only ran on the miss: a cache hit never
    // reaches the decorator sitting inside the cache.
    EXPECT_EQ(call_log.size(), 1u);
}

TEST(SurchargeRateSourceTest, AddsASurchargeProportionalToDeclaredValue) {
    SurchargeRateSource surcharge(std::make_unique<TariffTableRateSource>(), /*cents_per_declared_dollar=*/1.5);
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    // Base 940 + (declared $100 * 1.5 cents/dollar) = 940 + 150 = 1090.
    EXPECT_EQ(surcharge.quote(shipment), 1090);
}

TEST(DecoratorStackTest, FullStackProducesCoreTariffPlusSurcharge) {
    std::vector<std::string> call_log;
    auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
    auto caching = std::make_unique<CachingRateSource>(std::move(logging));
    SurchargeRateSource surcharge(std::move(caching), /*cents_per_declared_dollar=*/1.5);
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    EXPECT_EQ(surcharge.quote(shipment), 1090);
}

TEST(DecoratorStackTest, SurchargeAppliesFreshlyEvenWhenTheBaseRateIsCached) {
    std::vector<std::string> call_log;
    auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
    auto caching_owned = std::make_unique<CachingRateSource>(std::move(logging));
    CachingRateSource* caching = caching_owned.get();
    auto surcharge =
        std::make_unique<SurchargeRateSource>(std::move(caching_owned), /*cents_per_declared_dollar=*/1.5);
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    const Cents first = surcharge->quote(shipment);
    surcharge->set_cents_per_declared_dollar(3.0);
    const Cents second = surcharge->quote(shipment);

    EXPECT_EQ(caching->cache_misses(), 1u);
    EXPECT_EQ(caching->cache_hits(), 1u);
    EXPECT_EQ(call_log.size(), 1u);
    // Surcharge sits outside the cache, so a later rate change shows up
    // immediately even though the base rate underneath it was memoized.
    EXPECT_EQ(first, 1090);
    EXPECT_EQ(second, 1240);
    EXPECT_NE(first, second);
}

TEST(DecoratorStackTest, ReorderingCachingOutsideSurchargeMemoizesTheStaleSurcharge) {
    std::vector<std::string> call_log;
    auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
    auto surcharge_owned =
        std::make_unique<SurchargeRateSource>(std::move(logging), /*cents_per_declared_dollar=*/1.5);
    SurchargeRateSource* surcharge = surcharge_owned.get();
    auto caching = std::make_unique<CachingRateSource>(std::move(surcharge_owned));
    const Shipment shipment{2.0, DestinationZone::Regional, 10000};

    const Cents first = caching->quote(shipment);
    surcharge->set_cents_per_declared_dollar(3.0);
    const Cents second = caching->quote(shipment);

    EXPECT_EQ(caching->cache_misses(), 1u);
    EXPECT_EQ(caching->cache_hits(), 1u);
    EXPECT_EQ(call_log.size(), 1u);
    // Swapping the two decorators' order, with nothing else changed, now
    // caches the already-surcharged total, so the rate change the second
    // test observed immediately has no effect here until the shipment
    // key itself changes.
    EXPECT_EQ(first, 1090);
    EXPECT_EQ(second, 1090);
}

}  // namespace
}  // namespace shipping
