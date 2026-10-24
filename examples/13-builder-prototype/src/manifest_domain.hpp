#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace manifest {

// Money in this domain is tracked as integer cents, never floating point,
// so a quote or a declared value is exact and reproducible in a test
// assertion, the same convention Chapter 8's shipping example uses.
using Cents = std::int64_t;

enum class ShippingPriority { Standard, Expedited, Overnight };

std::string_view to_string(ShippingPriority priority);
ShippingPriority priority_from_string(std::string_view text);

class PackageTemplate;

// The product ManifestBuilder assembles: a shipment header plus the
// packages riding inside it. Packages are held as owning pointers to a
// polymorphic PackageTemplate because each concrete package kind (fragile
// electronics, bulk dry goods, refrigerated perishable) carries its own
// clone() behavior; see package_prototype.hpp. That makes ShipmentManifest
// itself move-only, which is fine: a manifest is produced once, by a
// builder or by deserialize_manifest(), and handed off by value via move.
struct ShipmentManifest {
    std::string shipment_id;
    std::string carrier;
    std::string destination;
    ShippingPriority priority = ShippingPriority::Standard;
    Cents insured_value_cents = 0;
    std::string special_handling_notes;
    std::vector<std::unique_ptr<PackageTemplate>> packages;
};

// Structural equality across a polymorphic, move-only manifest: compares
// every header field plus each package's kind(), weight_kg(),
// handling_code(), and declared_value_cents() pairwise. This is what lets
// the serialization round-trip test compare the manifest it built against
// the manifest deserialize_manifest() parsed back out of plain text,
// without ShipmentManifest ever needing a copy constructor.
bool manifests_equal(const ShipmentManifest& lhs, const ShipmentManifest& rhs);

}  // namespace manifest
