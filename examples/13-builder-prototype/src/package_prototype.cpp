#include "package_prototype.hpp"

#include <utility>

namespace manifest {

PackageTemplate::PackageTemplate(double weight_kg, std::string handling_code, Cents declared_value_cents)
    : weight_kg_(weight_kg), handling_code_(std::move(handling_code)), declared_value_cents_(declared_value_cents) {}

FragileElectronicsPackage::FragileElectronicsPackage(double weight_kg, Cents declared_value_cents)
    : PackageTemplate(weight_kg, "FRAGILE", declared_value_cents) {}

std::unique_ptr<PackageTemplate> FragileElectronicsPackage::clone() const {
    // The compiler-generated copy constructor (inherited via the
    // protected PackageTemplate(const PackageTemplate&) above) is a deep
    // copy here: every member is a value type (double, std::string,
    // Cents), none of them an owned pointer this class would otherwise
    // have to clone by hand.
    return std::make_unique<FragileElectronicsPackage>(*this);
}

std::string_view FragileElectronicsPackage::kind() const {
    return "fragile-electronics";
}

BulkDryGoodsPackage::BulkDryGoodsPackage(double weight_kg, Cents declared_value_cents)
    : PackageTemplate(weight_kg, "STANDARD", declared_value_cents) {}

std::unique_ptr<PackageTemplate> BulkDryGoodsPackage::clone() const {
    return std::make_unique<BulkDryGoodsPackage>(*this);
}

std::string_view BulkDryGoodsPackage::kind() const {
    return "bulk-dry-goods";
}

RefrigeratedPerishablePackage::RefrigeratedPerishablePackage(double weight_kg, Cents declared_value_cents)
    : PackageTemplate(weight_kg, "FROZEN", declared_value_cents) {}

std::unique_ptr<PackageTemplate> RefrigeratedPerishablePackage::clone() const {
    return std::make_unique<RefrigeratedPerishablePackage>(*this);
}

std::string_view RefrigeratedPerishablePackage::kind() const {
    return "refrigerated-perishable";
}

std::unique_ptr<PackageTemplate> make_package(std::string_view kind, double weight_kg, Cents declared_value_cents) {
    if (kind == "fragile-electronics") {
        return std::make_unique<FragileElectronicsPackage>(weight_kg, declared_value_cents);
    }
    if (kind == "bulk-dry-goods") {
        return std::make_unique<BulkDryGoodsPackage>(weight_kg, declared_value_cents);
    }
    if (kind == "refrigerated-perishable") {
        return std::make_unique<RefrigeratedPerishablePackage>(weight_kg, declared_value_cents);
    }
    throw std::invalid_argument("make_package: unrecognized package kind '" + std::string(kind) + "'");
}

void PackageTemplateRegistry::register_template(std::string name, std::unique_ptr<PackageTemplate> prototype) {
    prototypes_[std::move(name)] = std::move(prototype);
}

std::unique_ptr<PackageTemplate> PackageTemplateRegistry::create(const std::string& name) const {
    const auto it = prototypes_.find(name);
    if (it == prototypes_.end()) {
        throw std::out_of_range("PackageTemplateRegistry::create: no prototype registered under '" + name + "'");
    }
    return it->second->clone();
}

bool PackageTemplateRegistry::contains(const std::string& name) const {
    return prototypes_.contains(name);
}

PackageTemplateRegistry default_package_template_registry() {
    PackageTemplateRegistry registry;
    registry.register_template("fragile-electronics", make_package("fragile-electronics", 3.50, 180000));
    registry.register_template("bulk-dry-goods", make_package("bulk-dry-goods", 22.00, 35000));
    registry.register_template("refrigerated-perishable", make_package("refrigerated-perishable", 14.75, 60000));
    return registry;
}

}  // namespace manifest
