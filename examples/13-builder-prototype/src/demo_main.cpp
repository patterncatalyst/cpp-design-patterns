#include <iostream>
#include <string>
#include <utility>

#include "manifest_builder.hpp"
#include "manifest_serialization.hpp"
#include "package_prototype.hpp"

#if __cplusplus >= 202302L
#include <print>
#endif

using manifest::Cents;
using manifest::ManifestBuilder;
using manifest::PackageTemplateRegistry;
using manifest::ShippingPriority;

int main() {
    std::cout << "== Builder: assembling a shipment manifest ==\n";

    PackageTemplateRegistry registry = manifest::default_package_template_registry();

    auto electronics = registry.create("fragile-electronics");
    electronics->set_weight_kg(4.50);
    electronics->set_declared_value_cents(120000);

    auto dry_goods = registry.create("bulk-dry-goods");
    dry_goods->set_weight_kg(22.00);

    manifest::ShipmentManifest built_manifest = ManifestBuilder()
                                                     .shipment_id("SHIP-4471")
                                                     .carrier("Northline Freight")
                                                     .destination("Columbus, OH")
                                                     .priority(ShippingPriority::Expedited)
                                                     .insured_value_cents(150000)
                                                     .special_handling_notes("Signature required on delivery")
                                                     .add_package(std::move(electronics))
                                                     .add_package(std::move(dry_goods))
                                                     .build();

    std::cout << manifest::summary_line(built_manifest) << "\n";

    std::cout << "\n== Builder: rejecting an incomplete manifest ==\n";
    try {
        ManifestBuilder().carrier("Northline Freight").destination("Columbus, OH").build();
    } catch (const std::invalid_argument& error) {
        std::cout << "Rejected: " << error.what() << "\n";
    }

    std::cout << "\n== Prototype: cloning independent copies from one registry ==\n";
    auto first_clone = registry.create("refrigerated-perishable");
    auto second_clone = registry.create("refrigerated-perishable");
    std::cout << "Before mutation: both clones report " << first_clone->weight_kg() << " kg\n";
    first_clone->set_weight_kg(9.25);
    std::cout << "After mutating only the first clone: first=" << first_clone->weight_kg() << " kg, second=" << second_clone->weight_kg()
              << " kg\n";

    std::cout << "\n== Serialization: round-tripping the manifest to text and back ==\n";
    const std::string serialized = manifest::serialize_manifest(built_manifest);
    std::cout << serialized;

    manifest::ShipmentManifest round_tripped = manifest::deserialize_manifest(serialized);
    const bool equal = manifest::manifests_equal(built_manifest, round_tripped);
    std::cout << "Round trip reproduced an equal manifest: " << (equal ? "yes" : "no") << "\n";

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: std::print instead of std::format + std::cout ==\n";
    manifest::print_summary(round_tripped);
#endif

    return 0;
}
