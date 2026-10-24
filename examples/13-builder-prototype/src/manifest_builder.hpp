#pragma once

#include <memory>
#include <string>
#include <vector>

#include "manifest_domain.hpp"
#include "package_prototype.hpp"

namespace manifest {

// Builder: assembles a ShipmentManifest step by step through a fluent
// interface. shipment_id(), carrier(), and destination() are required;
// priority(), insured_value_cents(), and special_handling_notes() are
// optional and already hold their defaults (Standard, 0, empty) before a
// caller ever touches them. Nothing is validated until build() runs, so a
// caller can set fields in whatever order is convenient and add packages
// one at a time as it decides what the shipment actually contains.
class ManifestBuilder {
public:
    ManifestBuilder& shipment_id(std::string id);
    ManifestBuilder& carrier(std::string carrier_name);
    ManifestBuilder& destination(std::string destination_name);
    ManifestBuilder& priority(ShippingPriority priority_value);
    ManifestBuilder& insured_value_cents(Cents cents);
    ManifestBuilder& special_handling_notes(std::string notes);
    ManifestBuilder& add_package(std::unique_ptr<PackageTemplate> package);

    // Validates the manifest assembled so far and moves it out of this
    // builder. Throws std::invalid_argument if shipment_id, carrier, or
    // destination is empty, or if no package has been added. After
    // build() returns, this builder holds an empty manifest again and can
    // be reused to assemble another one.
    ShipmentManifest build();

private:
    ShipmentManifest manifest_;

    // Empty string means "valid"; this keeps the one validation rule set
    // in one place, checked identically whether build() succeeds or
    // throws.
    std::string validation_error() const;
};

}  // namespace manifest
