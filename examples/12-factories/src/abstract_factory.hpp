#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "shipping_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace carrier_integration {

// Abstract products. ShipmentProcessor below depends on these three
// interfaces only; it never names FastShip or SummitFreight directly.
class LabelFormatter {
public:
    virtual ~LabelFormatter() = default;
    virtual std::string format_label(const Shipment& shipment) const = 0;
};

class RateQuoter {
public:
    virtual ~RateQuoter() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};

class TrackingClient {
public:
    virtual ~TrackingClient() = default;
    virtual std::string tracking_url(const std::string& tracking_number) const = 0;
};

// Abstract Factory: produces one matching family of the three products
// above per carrier. Building all three from the same factory call is what
// guarantees the family agrees on account id, label format, and tracking
// domain; nothing enforces that agreement if the three products are
// constructed separately, from three different places in the codebase.
class CarrierIntegrationFactory {
public:
    virtual ~CarrierIntegrationFactory() = default;
    virtual std::unique_ptr<LabelFormatter> make_label_formatter() const = 0;
    virtual std::unique_ptr<RateQuoter> make_rate_quoter() const = 0;
    virtual std::unique_ptr<TrackingClient> make_tracking_client() const = 0;
};

class FastShipIntegrationFactory : public CarrierIntegrationFactory {
public:
    explicit FastShipIntegrationFactory(std::string account_id);
    std::unique_ptr<LabelFormatter> make_label_formatter() const override;
    std::unique_ptr<RateQuoter> make_rate_quoter() const override;
    std::unique_ptr<TrackingClient> make_tracking_client() const override;

private:
    std::string account_id_;
};

class SummitFreightIntegrationFactory : public CarrierIntegrationFactory {
public:
    explicit SummitFreightIntegrationFactory(std::string account_id);
    std::unique_ptr<LabelFormatter> make_label_formatter() const override;
    std::unique_ptr<RateQuoter> make_rate_quoter() const override;
    std::unique_ptr<TrackingClient> make_tracking_client() const override;

private:
    std::string account_id_;
};

// Client: holds one LabelFormatter, one RateQuoter, and one TrackingClient,
// all built from the same CarrierIntegrationFactory in the constructor.
// After that point there is no way for one carrier's label formatter to
// end up paired with another carrier's rate quoter; the three products
// came from one factory call and stay together for the processor's
// lifetime.
struct ProcessedShipment {
    std::string label;
    Cents rate_cents;
    std::string tracking_url;
};

class ShipmentProcessor {
public:
    explicit ShipmentProcessor(const CarrierIntegrationFactory& factory);
    ProcessedShipment process(const Shipment& shipment, const std::string& tracking_number) const;

private:
    std::unique_ptr<LabelFormatter> label_formatter_;
    std::unique_ptr<RateQuoter> rate_quoter_;
    std::unique_ptr<TrackingClient> tracking_client_;
};

// Carrier lookup by name, the call a merchant's checkout makes once per
// order after reading the chosen carrier out of configuration.
// The C++20 path throws on an unrecognized name; the C++23 refinement
// below reports the same failure as a typed error instead.
std::unique_ptr<CarrierIntegrationFactory> make_carrier_factory(std::string_view carrier_name, std::string account_id);

#if __cplusplus >= 202302L
// C++23 refinement: reject an unrecognized carrier name with a typed error
// instead of an exception. Guarded so the mandatory C++20 build path never
// requires std::expected.
std::expected<std::unique_ptr<CarrierIntegrationFactory>, std::string> try_make_carrier_factory(
    std::string_view carrier_name, std::string account_id);
#endif

}  // namespace carrier_integration
