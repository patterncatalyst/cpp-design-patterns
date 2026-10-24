// Adapter, Facade, Bridge, and PImpl in one shipping subsystem. Build and
// run via scripts/build.sh from the repo root, or standalone from this
// directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>

#include "label_renderer.hpp"
#include "legacy_rate_api.hpp"
#include "notification.hpp"
#include "rate_adapter.hpp"
#include "shipping_domain.hpp"
#include "shipping_facade.hpp"

int main() {
    using namespace shipping;

    const Shipment parcel{/*weight_kg=*/3.5, DestinationZone::Regional, /*declared_value_cents=*/12000};

    std::cout << "== Adapter: the legacy GlobalFreight API behind RateSource ==\n";
    auto rate_source = std::make_unique<LegacyFreightRateAdapter>(
        legacy_freight::LegacyGlobalFreightRateApi(/*surcharge_cents=*/50));
    std::cout << "Adapted quote: " << rate_source->quote(parcel) << " cents\n";

    std::cout << "\n== Facade: one call fronts rating, labeling, and tracking ==\n";
    ShippingFacade facade(
        std::make_unique<LegacyFreightRateAdapter>(legacy_freight::LegacyGlobalFreightRateApi(/*surcharge_cents=*/50)),
        LabelRenderer("GlobalFreight"), TrackingRegistry("track.globalfreight.example"));
    const ShippingQuote quote = facade.quote_and_label(parcel, /*sequence=*/1);
    std::cout << "price:    " << quote.price_cents << " cents\n";
    std::cout << "label:    " << quote.label << "\n";
    std::cout << "tracking: " << quote.tracking_url << "\n";

    std::cout << "\n== PImpl: LabelRenderer copies correctly, header hides its format ==\n";
    LabelRenderer original("GlobalFreight");
    LabelRenderer copy = original;
    std::cout << "original: " << original.render(parcel, "TRK-9001") << "\n";
    std::cout << "copy:     " << copy.render(parcel, "TRK-9001") << "\n";

    std::cout << "\n== Bridge: two notification kinds, two transports, no class explosion ==\n";
    auto email = std::make_shared<EmailTransport>();
    auto sms = std::make_shared<SmsTransport>();
    ShipmentCreatedNotification created_email(email, quote.tracking_number);
    ShipmentCreatedNotification created_sms(sms, quote.tracking_number);
    DeliveryDelayedNotification delayed_email(email, quote.tracking_number, /*delay_hours=*/6);
    DeliveryDelayedNotification delayed_sms(sms, quote.tracking_number, /*delay_hours=*/6);
    std::cout << created_email.send("customer@example.com") << "\n";
    std::cout << created_sms.send("+1-555-0100") << "\n";
    std::cout << delayed_email.send("customer@example.com") << "\n";
    std::cout << delayed_sms.send("+1-555-0100") << "\n";

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed rejection via std::expected ==\n";
    const Shipment malformed{/*weight_kg=*/-1.0, DestinationZone::Local, 0};
    if (const auto result = facade.try_quote_and_label(malformed, 2); !result.has_value()) {
        std::cout << "Rejected malformed shipment: " << result.error() << "\n";
    }
#endif

    return 0;
}
