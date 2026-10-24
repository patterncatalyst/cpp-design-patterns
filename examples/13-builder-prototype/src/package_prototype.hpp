#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "manifest_domain.hpp"

namespace manifest {

// Prototype: a package carries enough state (weight, handling code,
// declared value) that building one from scratch for every order repeats
// the same few lines a dozen times over. A PackageTemplate instead clones
// itself into a fresh, independent instance through clone(); the thing
// being cloned is a pre-configured example of a package kind, not a blank
// constructor argument list.
class PackageTemplate {
public:
    virtual ~PackageTemplate() = default;

    // Returns a new, independently owned copy of this package. Cloning is
    // always a copy, never a shared reference: nothing downstream of
    // create() can mutate another order's package through this one.
    virtual std::unique_ptr<PackageTemplate> clone() const = 0;

    virtual std::string_view kind() const = 0;

    double weight_kg() const { return weight_kg_; }
    void set_weight_kg(double weight_kg) { weight_kg_ = weight_kg; }

    std::string_view handling_code() const { return handling_code_; }

    Cents declared_value_cents() const { return declared_value_cents_; }
    void set_declared_value_cents(Cents cents) { declared_value_cents_ = cents; }

protected:
    PackageTemplate(double weight_kg, std::string handling_code, Cents declared_value_cents);
    PackageTemplate(const PackageTemplate&) = default;

private:
    double weight_kg_;
    std::string handling_code_;
    Cents declared_value_cents_;
};

class FragileElectronicsPackage : public PackageTemplate {
public:
    FragileElectronicsPackage(double weight_kg, Cents declared_value_cents);
    std::unique_ptr<PackageTemplate> clone() const override;
    std::string_view kind() const override;
};

class BulkDryGoodsPackage : public PackageTemplate {
public:
    BulkDryGoodsPackage(double weight_kg, Cents declared_value_cents);
    std::unique_ptr<PackageTemplate> clone() const override;
    std::string_view kind() const override;
};

class RefrigeratedPerishablePackage : public PackageTemplate {
public:
    RefrigeratedPerishablePackage(double weight_kg, Cents declared_value_cents);
    std::unique_ptr<PackageTemplate> clone() const override;
    std::string_view kind() const override;
};

// Constructs a package of the named kind directly, with caller-supplied
// weight and declared value rather than a template's defaults. Both
// default_package_template_registry() (below) and
// deserialize_manifest() (manifest_serialization.hpp) call this; the
// registry calls it once per prototype at startup, deserialization calls
// it once per PACKAGE line it parses. Throws std::invalid_argument for an
// unrecognized kind.
std::unique_ptr<PackageTemplate> make_package(std::string_view kind, double weight_kg, Cents declared_value_cents);

// Prototype: a small, named registry of pre-configured package templates.
// create() clones the named prototype into a fresh instance; the
// registry's own copy is never handed out and never changes.
class PackageTemplateRegistry {
public:
    void register_template(std::string name, std::unique_ptr<PackageTemplate> prototype);

    // Throws std::out_of_range if no prototype is registered under name.
    std::unique_ptr<PackageTemplate> create(const std::string& name) const;

    bool contains(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<PackageTemplate>> prototypes_;
};

// Populates a registry with this book's three named prototypes:
// "fragile-electronics", "bulk-dry-goods", and "refrigerated-perishable",
// each carrying a representative default weight and declared value a
// dispatcher can clone and adjust per order.
PackageTemplateRegistry default_package_template_registry();

}  // namespace manifest
