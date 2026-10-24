#include "abstract_factory.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace carrier_integration {

namespace {

std::string lowercase(std::string_view text) {
    std::string result(text);
    for (char& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

// ---- FastShip family: a parcel carrier billing by base fee + weight. ----

class FastShipLabelFormatter : public LabelFormatter {
public:
    explicit FastShipLabelFormatter(std::string account_id) : account_id_(std::move(account_id)) {}

    std::string format_label(const Shipment& shipment) const override {
        std::ostringstream label;
        label << "FASTSHIP-LABEL acct=" << account_id_ << " dest=" << shipment.destination_country
              << " wt=" << shipment.weight_kg << "kg";
        return label.str();
    }

private:
    std::string account_id_;
};

class FastShipRateQuoter : public RateQuoter {
public:
    // FastShip's flat-plus-per-kilogram rate does not depend on the
    // account id; it is included in the family purely because every
    // carrier's product family always carries the same three roles.
    Cents quote(const Shipment& shipment) const override {
        const Cents base_cents = 350;
        const auto per_kg_cents = static_cast<Cents>(shipment.weight_kg * 180.0);
        return base_cents + per_kg_cents;
    }
};

class FastShipTrackingClient : public TrackingClient {
public:
    explicit FastShipTrackingClient(std::string account_id) : account_id_(std::move(account_id)) {}

    std::string tracking_url(const std::string& tracking_number) const override {
        return "https://track.fastship.example/" + account_id_ + "/" + tracking_number;
    }

private:
    std::string account_id_;
};

// ---- SummitFreight family: a freight carrier billing by weight + declared-value tier. ----

class SummitFreightLabelFormatter : public LabelFormatter {
public:
    explicit SummitFreightLabelFormatter(std::string account_id) : account_id_(std::move(account_id)) {}

    std::string format_label(const Shipment& shipment) const override {
        std::ostringstream label;
        label << "SF-MANIFEST account:" << account_id_ << " to:" << shipment.destination_country
              << " weight:" << shipment.weight_kg;
        return label.str();
    }

private:
    std::string account_id_;
};

class SummitFreightRateQuoter : public RateQuoter {
public:
    // SummitFreight prices by weight plus a declared-value tier, heavier
    // per kilogram than FastShip, reflecting its freight focus. Like
    // FastShip's quoter, it carries no account state of its own.
    Cents quote(const Shipment& shipment) const override {
        const auto per_kg_cents = static_cast<Cents>(shipment.weight_kg * 240.0);
        const Cents value_tier_cents = shipment.declared_value_cents > 10000 ? 500 : 200;
        return per_kg_cents + value_tier_cents;
    }
};

class SummitFreightTrackingClient : public TrackingClient {
public:
    explicit SummitFreightTrackingClient(std::string account_id) : account_id_(std::move(account_id)) {}

    std::string tracking_url(const std::string& tracking_number) const override {
        return "https://tracking.summitfreight.example/shipments/" + account_id_ + "-" + tracking_number;
    }

private:
    std::string account_id_;
};

}  // namespace

FastShipIntegrationFactory::FastShipIntegrationFactory(std::string account_id) : account_id_(std::move(account_id)) {}

std::unique_ptr<LabelFormatter> FastShipIntegrationFactory::make_label_formatter() const {
    return std::make_unique<FastShipLabelFormatter>(account_id_);
}

std::unique_ptr<RateQuoter> FastShipIntegrationFactory::make_rate_quoter() const {
    return std::make_unique<FastShipRateQuoter>();
}

std::unique_ptr<TrackingClient> FastShipIntegrationFactory::make_tracking_client() const {
    return std::make_unique<FastShipTrackingClient>(account_id_);
}

SummitFreightIntegrationFactory::SummitFreightIntegrationFactory(std::string account_id)
    : account_id_(std::move(account_id)) {}

std::unique_ptr<LabelFormatter> SummitFreightIntegrationFactory::make_label_formatter() const {
    return std::make_unique<SummitFreightLabelFormatter>(account_id_);
}

std::unique_ptr<RateQuoter> SummitFreightIntegrationFactory::make_rate_quoter() const {
    return std::make_unique<SummitFreightRateQuoter>();
}

std::unique_ptr<TrackingClient> SummitFreightIntegrationFactory::make_tracking_client() const {
    return std::make_unique<SummitFreightTrackingClient>(account_id_);
}

ShipmentProcessor::ShipmentProcessor(const CarrierIntegrationFactory& factory)
    : label_formatter_(factory.make_label_formatter()),
      rate_quoter_(factory.make_rate_quoter()),
      tracking_client_(factory.make_tracking_client()) {}

ProcessedShipment ShipmentProcessor::process(const Shipment& shipment, const std::string& tracking_number) const {
    return ProcessedShipment{
        label_formatter_->format_label(shipment),
        rate_quoter_->quote(shipment),
        tracking_client_->tracking_url(tracking_number),
    };
}

std::unique_ptr<CarrierIntegrationFactory> make_carrier_factory(std::string_view carrier_name, std::string account_id) {
    const std::string normalized = lowercase(carrier_name);
    if (normalized == "fastship") {
        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));
    }
    if (normalized == "summitfreight") {
        return std::make_unique<SummitFreightIntegrationFactory>(std::move(account_id));
    }
    throw std::invalid_argument("unknown carrier name: " + std::string(carrier_name));
}

#if __cplusplus >= 202302L
std::expected<std::unique_ptr<CarrierIntegrationFactory>, std::string> try_make_carrier_factory(
    std::string_view carrier_name, std::string account_id) {
    const std::string normalized = lowercase(carrier_name);
    if (normalized == "fastship") {
        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));
    }
    if (normalized == "summitfreight") {
        return std::make_unique<SummitFreightIntegrationFactory>(std::move(account_id));
    }
    return std::unexpected<std::string>("unknown carrier name: " + std::string(carrier_name));
}
#endif

}  // namespace carrier_integration
