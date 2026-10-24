#include "shipping_facade.hpp"

#include <stdexcept>
#include <utility>

namespace shipping {

TrackingRegistry::TrackingRegistry(std::string carrier_domain) : carrier_domain_(std::move(carrier_domain)) {}

std::string TrackingRegistry::issue_tracking_number(int sequence) const {
    return "TRK-" + std::to_string(1000 + sequence);
}

std::string TrackingRegistry::tracking_url(const std::string& tracking_number) const {
    return "https://" + carrier_domain_ + "/track/" + tracking_number;
}

ShippingFacade::ShippingFacade(std::unique_ptr<RateSource> rate_source, LabelRenderer label_renderer,
                                TrackingRegistry tracking_registry)
    : rate_source_(std::move(rate_source)),
      label_renderer_(std::move(label_renderer)),
      tracking_registry_(std::move(tracking_registry)) {}

ShippingQuote ShippingFacade::quote_and_label(const Shipment& shipment, int sequence) const {
    if (shipment.weight_kg < 0.0) {
        throw std::invalid_argument("shipment weight_kg cannot be negative");
    }

    const Cents price = rate_source_->quote(shipment);
    const std::string tracking_number = tracking_registry_.issue_tracking_number(sequence);
    const std::string label = label_renderer_.render(shipment, tracking_number);
    const std::string url = tracking_registry_.tracking_url(tracking_number);
    return ShippingQuote{price, label, tracking_number, url};
}

#if __cplusplus >= 202302L
std::expected<ShippingQuote, std::string> ShippingFacade::try_quote_and_label(const Shipment& shipment,
                                                                               int sequence) const {
    if (shipment.weight_kg < 0.0) {
        return std::unexpected<std::string>("shipment weight_kg cannot be negative");
    }

    const Cents price = rate_source_->quote(shipment);
    const std::string tracking_number = tracking_registry_.issue_tracking_number(sequence);
    const std::string label = label_renderer_.render(shipment, tracking_number);
    const std::string url = tracking_registry_.tracking_url(tracking_number);
    return ShippingQuote{price, label, tracking_number, url};
}
#endif

}  // namespace shipping
