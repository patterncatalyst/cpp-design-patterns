#include "label_renderer.hpp"

#include <sstream>
#include <utility>

namespace shipping {

namespace {

const char* zone_name(DestinationZone zone) {
    switch (zone) {
        case DestinationZone::Local:
            return "Local";
        case DestinationZone::Regional:
            return "Regional";
        case DestinationZone::National:
            return "National";
        case DestinationZone::International:
            return "International";
    }
    return "Unknown";
}

}  // namespace

// Everything about the label's layout lives here, invisible to anything
// that only includes label_renderer.hpp.
class LabelRenderer::Impl {
public:
    explicit Impl(std::string carrier_name) : carrier_name_(std::move(carrier_name)) {}

    std::string render(const Shipment& shipment, const std::string& tracking_number) const {
        std::ostringstream label;
        label << carrier_name_ << " LABEL | weight=" << shipment.weight_kg << "kg"
              << " | zone=" << zone_name(shipment.zone) << " | tracking=" << tracking_number;
        return label.str();
    }

private:
    std::string carrier_name_;
};

LabelRenderer::LabelRenderer(std::string carrier_name) : impl_(std::make_unique<Impl>(std::move(carrier_name))) {}

LabelRenderer::~LabelRenderer() = default;

LabelRenderer::LabelRenderer(const LabelRenderer& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

LabelRenderer& LabelRenderer::operator=(const LabelRenderer& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

LabelRenderer::LabelRenderer(LabelRenderer&& other) noexcept = default;

LabelRenderer& LabelRenderer::operator=(LabelRenderer&& other) noexcept = default;

std::string LabelRenderer::render(const Shipment& shipment, const std::string& tracking_number) const {
    return impl_->render(shipment, tracking_number);
}

}  // namespace shipping
