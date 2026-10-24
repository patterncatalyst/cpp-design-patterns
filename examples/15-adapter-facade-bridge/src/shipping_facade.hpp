#pragma once

#include <memory>
#include <string>

#include "label_renderer.hpp"
#include "rate_adapter.hpp"
#include "shipping_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace shipping {

// Tracking subsystem. Deliberately small: the point of this example is how
// ShippingFacade composes it with rating and labeling, not what it does on
// its own.
class TrackingRegistry {
public:
    explicit TrackingRegistry(std::string carrier_domain);

    std::string issue_tracking_number(int sequence) const;
    std::string tracking_url(const std::string& tracking_number) const;

private:
    std::string carrier_domain_;
};

struct ShippingQuote {
    Cents price_cents;
    std::string label;
    std::string tracking_number;
    std::string tracking_url;
};

// Facade: a checkout calls quote_and_label() once. Rating (a RateSource,
// here the Adapter built above), labeling (a PImpl LabelRenderer), and
// tracking (TrackingRegistry) are three subsystems this facade owns; a
// caller never orchestrates them directly and never sees them as separate
// objects.
class ShippingFacade {
public:
    ShippingFacade(std::unique_ptr<RateSource> rate_source, LabelRenderer label_renderer,
                   TrackingRegistry tracking_registry);

    // Throws std::invalid_argument on a malformed shipment (negative
    // weight). See try_quote_and_label() below for the C++23 typed
    // alternative.
    ShippingQuote quote_and_label(const Shipment& shipment, int sequence) const;

#if __cplusplus >= 202302L
    // C++23 refinement: reject a malformed shipment with a typed error
    // instead of an exception. Guarded so the mandatory C++20 build path
    // never requires std::expected.
    std::expected<ShippingQuote, std::string> try_quote_and_label(const Shipment& shipment, int sequence) const;
#endif

private:
    std::unique_ptr<RateSource> rate_source_;
    LabelRenderer label_renderer_;
    TrackingRegistry tracking_registry_;
};

}  // namespace shipping
