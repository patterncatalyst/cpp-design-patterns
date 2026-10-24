#include "manifest_builder.hpp"

#include <stdexcept>
#include <utility>

namespace manifest {

ManifestBuilder& ManifestBuilder::shipment_id(std::string id) {
    manifest_.shipment_id = std::move(id);
    return *this;
}

ManifestBuilder& ManifestBuilder::carrier(std::string carrier_name) {
    manifest_.carrier = std::move(carrier_name);
    return *this;
}

ManifestBuilder& ManifestBuilder::destination(std::string destination_name) {
    manifest_.destination = std::move(destination_name);
    return *this;
}

ManifestBuilder& ManifestBuilder::priority(ShippingPriority priority_value) {
    manifest_.priority = priority_value;
    return *this;
}

ManifestBuilder& ManifestBuilder::insured_value_cents(Cents cents) {
    manifest_.insured_value_cents = cents;
    return *this;
}

ManifestBuilder& ManifestBuilder::special_handling_notes(std::string notes) {
    manifest_.special_handling_notes = std::move(notes);
    return *this;
}

ManifestBuilder& ManifestBuilder::add_package(std::unique_ptr<PackageTemplate> package) {
    manifest_.packages.push_back(std::move(package));
    return *this;
}

std::string ManifestBuilder::validation_error() const {
    if (manifest_.shipment_id.empty()) {
        return "ManifestBuilder: shipment_id is required";
    }
    if (manifest_.carrier.empty()) {
        return "ManifestBuilder: carrier is required";
    }
    if (manifest_.destination.empty()) {
        return "ManifestBuilder: destination is required";
    }
    if (manifest_.packages.empty()) {
        return "ManifestBuilder: at least one package is required";
    }
    return "";
}

ShipmentManifest ManifestBuilder::build() {
    const std::string error = validation_error();
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
    return std::exchange(manifest_, ShipmentManifest{});
}

}  // namespace manifest
