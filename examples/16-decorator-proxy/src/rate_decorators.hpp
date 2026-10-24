#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "shipping_domain.hpp"

namespace shipping {

// Decorator target interface. The core tariff lookup at the bottom of a
// stack and every layer wrapped around it implement this one interface, so
// a client holding a RateSource& cannot tell how many decorators, if any,
// sit between it and the core.
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) = 0;
};

// The core component: looks up the base tariff with no decoration at all.
// Every decorator below wraps something that implements RateSource, and
// at the bottom of any real stack, that something is this class.
class TariffTableRateSource : public RateSource {
public:
    Cents quote(const Shipment& shipment) override;
};

// Decorator: appends one line to a shared call log every time quote() runs,
// then delegates to the wrapped RateSource unchanged. Where this decorator
// sits in a stack determines whether an outer layer's cache hit ever
// reaches it: a cache sitting outside this decorator can skip the call
// entirely, and no line gets logged for that lookup.
class LoggingRateSource : public RateSource {
public:
    LoggingRateSource(std::unique_ptr<RateSource> inner, std::vector<std::string>& call_log);
    Cents quote(const Shipment& shipment) override;

private:
    std::unique_ptr<RateSource> inner_;
    std::vector<std::string>& call_log_;
};

// Decorator: remembers the quote for an identical shipment and returns it
// without calling the wrapped RateSource again on a repeat lookup. It
// memoizes whatever price the wrapped source currently returns, which
// includes any surcharge already baked into that price if a surcharge
// decorator sits inside this one rather than outside it.
class CachingRateSource : public RateSource {
public:
    explicit CachingRateSource(std::unique_ptr<RateSource> inner);
    Cents quote(const Shipment& shipment) override;

    std::size_t cache_hits() const { return cache_hits_; }
    std::size_t cache_misses() const { return cache_misses_; }

private:
    std::unique_ptr<RateSource> inner_;
    std::unordered_map<std::string, Cents> cache_;
    std::size_t cache_hits_ = 0;
    std::size_t cache_misses_ = 0;
};

// Decorator: adds a surcharge proportional to declared value on top of
// whatever the wrapped RateSource quotes. The rate is mutable through
// set_cents_per_declared_dollar() specifically so this example can show
// that a surcharge computed outside a cache applies fresh on every call,
// while one computed inside a cache gets memoized along with the base
// price and goes stale until the cached shipment's key changes.
class SurchargeRateSource : public RateSource {
public:
    SurchargeRateSource(std::unique_ptr<RateSource> inner, double cents_per_declared_dollar);
    Cents quote(const Shipment& shipment) override;

    void set_cents_per_declared_dollar(double cents_per_declared_dollar);

private:
    std::unique_ptr<RateSource> inner_;
    double cents_per_declared_dollar_;
};

}  // namespace shipping
