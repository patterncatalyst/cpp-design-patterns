#include "manifest_domain.hpp"

#include <stdexcept>

#include "package_prototype.hpp"

namespace manifest {

std::string_view to_string(ShippingPriority priority) {
    switch (priority) {
        case ShippingPriority::Standard:
            return "Standard";
        case ShippingPriority::Expedited:
            return "Expedited";
        case ShippingPriority::Overnight:
            return "Overnight";
    }
    throw std::invalid_argument("to_string(ShippingPriority): unrecognized enumerator");
}

ShippingPriority priority_from_string(std::string_view text) {
    if (text == "Standard") {
        return ShippingPriority::Standard;
    }
    if (text == "Expedited") {
        return ShippingPriority::Expedited;
    }
    if (text == "Overnight") {
        return ShippingPriority::Overnight;
    }
    throw std::invalid_argument("priority_from_string: unrecognized priority '" + std::string(text) + "'");
}

bool manifests_equal(const ShipmentManifest& lhs, const ShipmentManifest& rhs) {
    if (lhs.shipment_id != rhs.shipment_id || lhs.carrier != rhs.carrier || lhs.destination != rhs.destination) {
        return false;
    }
    if (lhs.priority != rhs.priority || lhs.insured_value_cents != rhs.insured_value_cents) {
        return false;
    }
    if (lhs.special_handling_notes != rhs.special_handling_notes) {
        return false;
    }
    if (lhs.packages.size() != rhs.packages.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.packages.size(); ++i) {
        const PackageTemplate& left = *lhs.packages[i];
        const PackageTemplate& right = *rhs.packages[i];
        if (left.kind() != right.kind() || left.weight_kg() != right.weight_kg() ||
            left.handling_code() != right.handling_code() ||
            left.declared_value_cents() != right.declared_value_cents()) {
            return false;
        }
    }

    return true;
}

}  // namespace manifest
