---
title: "Builder, Prototype, and serialization"
order: 13
part: "creating-objects-under-control"
description: "A fluent builder that assembles a multi-field shipment manifest and validates it only once, at build(), a small named registry of package prototypes that clone into fresh independent copies, and a plain-text serialization format that round-trips a manifest back to an equal value, taught against a freight dispatch domain."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force Builder answers: required fields, optional fields with defaults and a cross-field invariant that no single constructor expresses cleanly.
- Build a fluent `ManifestBuilder` whose setters return `*this`, and explain why validation belongs only in `build()`.
- Name the force Prototype answers: a few pre-configured kinds reused constantly, whose defaults shouldn't be repeated at every call site.
- Build a `PackageTemplateRegistry` that clones a named prototype into an independent copy, and say why the compiler-generated copy constructor gives a deep copy here.
- Round-trip a manifest through plain text and check the result with a structural comparison, not a string comparison.
- Recognize when a Builder or a Prototype does what a constructor or a factory function already does, and say what to write instead.

## Why this matters

A freight dispatcher fills in three kinds of field on a shipment manifest. A carrier and a destination are required. Priority and insurance are optional, and most shipments take the defaults. And across the whole manifest, zero packages is invalid however carefully the header was filled in. The packages themselves come in a few kinds, fragile electronics, bulk dry goods, refrigerated perishables, each with its own default weight and handling code, reused on order after order.

{% include excalidraw.html name="13-builder-prototype" caption="Figure 13.1: ManifestBuilder assembles a ShipmentManifest behind a fluent interface and validates it once at build(); PackageTemplateRegistry clones a named prototype into an independent copy; serialize_manifest() and deserialize_manifest() round-trip the result through plain text." %}

A `ShipmentManifest` carries three required header fields, three optional ones holding their defaults, and a list of packages:

```cpp
enum class ShippingPriority { Standard, Expedited, Overnight };
using Cents = std::int64_t;

struct ShipmentManifest {
    std::string shipment_id;
    std::string carrier;
    std::string destination;
    ShippingPriority priority = ShippingPriority::Standard;
    Cents insured_value_cents = 0;
    std::string special_handling_notes;
    std::vector<std::unique_ptr<PackageTemplate>> packages;
};
```

Money is integer `Cents`, so values compare exactly. `packages` holds owning pointers to a polymorphic `PackageTemplate`, because each kind has its own `clone()`. That makes `ShipmentManifest` move-only, which suits an object produced once, by a builder or by `deserialize_manifest()`, and handed on by value.

## Pattern: Builder

### The problem

A constructor taking every field forces callers to pass values they don't care about, in an order nobody remembers. An overload per combination of optional fields grows with every new field. Neither has a natural place to check that the manifest has at least one package.

### The solution

A **Builder** accumulates a product's fields one call at a time, applies defaults for what the caller skipped and checks the product's invariants once, when the caller asks for the finished object.

### How C++ models it

`ManifestBuilder` holds one `ShipmentManifest` and exposes a chained setter per field:

```cpp
class ManifestBuilder {
public:
    ManifestBuilder& shipment_id(std::string id);
    ManifestBuilder& carrier(std::string carrier_name);
    ManifestBuilder& destination(std::string destination_name);
    ManifestBuilder& priority(ShippingPriority priority_value);
    ManifestBuilder& insured_value_cents(Cents cents);
    ManifestBuilder& special_handling_notes(std::string notes);
    ManifestBuilder& add_package(std::unique_ptr<PackageTemplate> package);

    ShipmentManifest build();

private:
    ShipmentManifest manifest_;
};
```

Each setter returns `*this`, so a caller sets fields in whatever order reads best:

```cpp
ShipmentManifest built = ManifestBuilder()
    .shipment_id("SHIP-4471")
    .carrier("Northline Freight")
    .destination("Columbus, OH")
    .priority(ShippingPriority::Expedited)
    .add_package(registry.create("fragile-electronics"))
    .build();
```

Nothing is checked as the chain runs. `priority()` doesn't require `shipment_id()` first, and `add_package()` doesn't require a carrier. A half-built manifest isn't invalid, only unfinished. `build()` is where the caller declares it complete:

```cpp
std::string ManifestBuilder::validation_error() const {
    if (manifest_.shipment_id.empty()) return "ManifestBuilder: shipment_id is required";
    if (manifest_.carrier.empty()) return "ManifestBuilder: carrier is required";
    if (manifest_.destination.empty()) return "ManifestBuilder: destination is required";
    if (manifest_.packages.empty()) return "ManifestBuilder: at least one package is required";
    return "";
}

ShipmentManifest ManifestBuilder::build() {
    const std::string error = validation_error();
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
    return std::exchange(manifest_, ShipmentManifest{});
}
```

The invariant "a manifest leaving `build()` is complete" can fail in one function only. Put the checks in the setters instead and the builder reintroduces the fixed call order it exists to remove, and its error messages name an ordering rule rather than the missing field.

`std::exchange` moves the manifest out and resets the builder to a fresh `ShipmentManifest` in one statement, so the builder is ready for the next order. A plain `std::move(manifest_)` leaves the moved-from strings in an unspecified state and leaves `priority` and `insured_value_cents` behind, since moving an enum or an integer copies it.

## Pattern: Prototype

### The problem

Writing out a fragile-electronics package by hand at each call site copies the same weight, handling code and declared value dozens of times. Changing one default means finding every copy.

### The solution

A **Prototype** keeps one configured instance of each kind and makes new instances by cloning it.

### How C++ models it

`clone()` is the one Prototype responsibility on `PackageTemplate`:

```cpp
class PackageTemplate {
public:
    virtual ~PackageTemplate() = default;
    virtual std::unique_ptr<PackageTemplate> clone() const = 0;
    virtual std::string_view kind() const = 0;

    double weight_kg() const { return weight_kg_; }
    void set_weight_kg(double weight_kg) { weight_kg_ = weight_kg; }
    Cents declared_value_cents() const { return declared_value_cents_; }

protected:
    PackageTemplate(double weight_kg, std::string handling_code, Cents declared_value_cents);
    PackageTemplate(const PackageTemplate&) = default;

private:
    double weight_kg_;
    std::string handling_code_;
    Cents declared_value_cents_;
};
```

`FragileElectronicsPackage::clone()` is one line: `return std::make_unique<FragileElectronicsPackage>(*this);`. That's already a full deep copy, because every member is a value type: a `double`, a `std::string`, a `Cents`. The defaulted, protected copy constructor copies all of it. The deep copy comes from the data layout, not from the pattern. A package kind that held a `unique_ptr` member, say an attached document, would need a hand-written copy of that member in `clone()`.

`PackageTemplateRegistry` is the named catalogue a dispatcher clones from:

```cpp
class PackageTemplateRegistry {
public:
    void register_template(std::string name, std::unique_ptr<PackageTemplate> prototype);
    std::unique_ptr<PackageTemplate> create(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<PackageTemplate>> prototypes_;
};
```

`create("fragile-electronics")` clones the registered prototype and returns the copy. The registry's prototype is never handed out or changed, and an unknown name throws `std::out_of_range`. Two clones of the same name are independent: change one clone's `weight_kg` and the other clone, a fresh clone and the prototype all keep the default. A "registry" that handed out a shared pointer to one object would look right until two callers changed their copies.

### Serialization: clone versus construct

Something downstream almost always needs a manifest as text: a log line, a queue message, a file a trading partner reads. `serialize_manifest()` writes one `KEY=VALUE` line per header field and one `PACKAGE` line per package:

```cpp
std::string serialize_manifest(const ShipmentManifest& manifest_obj) {
    std::string text;
    text += std::format("SHIPMENT_ID={}\n", manifest_obj.shipment_id);
    text += std::format("CARRIER={}\n", manifest_obj.carrier);
    // ...destination, priority, insured value, notes follow the same shape
    for (const auto& package : manifest_obj.packages) {
        text += std::format("PACKAGE kind={} weight_kg={:.2f} handling_code={} declared_value_cents={}\n",
                             package->kind(), package->weight_kg(), package->handling_code(),
                             package->declared_value_cents());
    }
    return text;
}
```

`deserialize_manifest()` parses it back. It rebuilds each package with `make_package()`, a factory function keyed on `kind`, not through the registry. A deserialized package carries its own weight and declared value, which may differ from the prototype's defaults after a dispatcher adjusted them, so it's constructed from the parsed fields rather than cloned and patched. A malformed header line or an unknown key is rejected.

`ShipmentManifest` is move-only and has no `operator==`, so `manifests_equal()` compares structurally: every header field, then every package pairwise by `kind()`, `weight_kg()`, `handling_code()` and `declared_value_cents()`. Cloning reproduces a package from an object in memory; deserializing reproduces one from a flat string. Both have to agree on what "equivalent" means, and keeping that definition in one function stops the two drifting apart when the domain gains a field. A serialized manifest, with one package or several, deserializes to an equal manifest.

### Formatting: std::format or std::print

`summary_line()` builds a one-line summary as a `std::string`:

```cpp
TEST(ManifestSerializationTest, RoundTripReproducesAnEqualManifestWithMultiplePackages) {
    const ShipmentManifest original = sample_manifest();
    const std::string serialized = serialize_manifest(original);
    const ShipmentManifest round_tripped = deserialize_manifest(serialized);

    EXPECT_TRUE(manifests_equal(original, round_tripped));
}
```

That's what a caller wants for a log line or a comparison. A caller that only prints it builds a string it never uses. `std::print` and `std::println`, new in C++23, format straight to `stdout`. `print_summary()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never needs `<print>`:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: build the string with std::format, then hand it to std::cout
// separately. Correct, but the caller manages an intermediate buffer it
// only ever intended to print.
std::cout << summary_line(manifest) << "\n";
```
```cpp
// C++23: std::println formats and writes to stdout in the same call,
// using the same format string and arguments summary_line() does, with
// no std::string ever materialized in between.
void print_summary(const ShipmentManifest& manifest_obj) {
    std::println("{} via {} to {} [{}], {} package(s), insured for ${}.{:02d}",
                  manifest_obj.shipment_id, manifest_obj.carrier, manifest_obj.destination,
                  to_string(manifest_obj.priority), manifest_obj.packages.size(),
                  manifest_obj.insured_value_cents / 100, manifest_obj.insured_value_cents % 100);
}
```

Neither pattern changes shape between the standards. The difference is one call site choosing between a reusable string and a direct write.

## When not to use it

- **Builder for a small product.** A manifest with two required fields and no optional ones needs a constructor or an aggregate initializer. Required constructor parameters let the type system enforce what `build()` checks at runtime.
- **Builder with nothing to validate.** If callers set two or three fields and there's no cross-field invariant, a few factory functions that each return a complete object read better than a chain used two calls deep. Builder's value is concentrated in `build()`.
- **Prototype for one kind.** With one package kind, or packages that differ on every order, there's nothing to clone. A plain `Package` struct filled in at the call site beats a `PackageTemplate` hierarchy with one subclass.

> **🤖 Explore with your agent**
>
> "In `examples/13-builder-prototype`, add `HazardousMaterialsPackage` deriving from `PackageTemplate`, with its own `kind()` of `"hazardous-materials"`, a `clone()` that returns `std::make_unique<HazardousMaterialsPackage>(*this)`, and a `make_package()` branch so it survives `serialize_manifest()` and `deserialize_manifest()`. Register it in `PackageTemplateRegistry`. Add `ManifestBuilder::hazmat_contact(std::string)` and check in `validation_error()`, not in any setter, that a manifest with a hazardous package has a contact. Add tests that two clones are independent after one is changed, that `build()` throws `std::invalid_argument` without the contact, and that a round trip gives `manifests_equal()`."
>
> Run `ctest --preset cpp23` and check that no setter in `ManifestBuilder` throws.

## Build, run, observe

From the repository root, the superbuild picks up `examples/13-builder-prototype`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 19 cases pass under `cpp23` and 17 under `cpp20`; the two `std::print` cases compile only under C++23. To see the builder, the registry and the round trip run:

```sh
examples/13-builder-prototype/demo.sh cpp23
```

It assembles a manifest, rejects an incomplete one, clones two packages from one prototype, and round-trips the manifest through text:

```
== Builder: assembling a shipment manifest ==
SHIP-4471 via Northline Freight to Columbus, OH [Expedited], 2 package(s), insured for $1500.00

== Builder: rejecting an incomplete manifest ==
Rejected: ManifestBuilder: shipment_id is required

== Prototype: cloning independent copies from one registry ==
Before mutation: both clones report 14.75 kg
After mutating only the first clone: first=9.25 kg, second=14.75 kg

== Serialization: round-tripping the manifest to text and back ==
SHIPMENT_ID=SHIP-4471
CARRIER=Northline Freight
DESTINATION=Columbus, OH
PRIORITY=Expedited
INSURED_VALUE_CENTS=150000
NOTES=Signature required on delivery
PACKAGE kind=fragile-electronics weight_kg=4.50 handling_code=FRAGILE declared_value_cents=120000
PACKAGE kind=bulk-dry-goods weight_kg=22.00 handling_code=STANDARD declared_value_cents=35000
Round trip reproduced an equal manifest: yes

== C++23 refinement: std::print instead of std::format + std::cout ==
SHIP-4471 via Northline Freight to Columbus, OH [Expedited], 2 package(s), insured for $1500.00
```

The second clone still weighs 14.75 kg after the first changed to 9.25 kg. Under `cpp20` the output stops before the `std::print` block.

- **Validate in a setter.** In a scratch copy, make `ManifestBuilder::carrier()` throw when `shipment_id` is still empty. One case fails, and the demo's rejection now reads `Rejected: shipment_id first`, an ordering rule instead of the missing field.
- **Move instead of exchange.** In a scratch copy, change `build()` to `return std::move(manifest_);`. The builder-reuse case fails: the second manifest keeps the first one's `Overnight` priority and insured value.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Builder and Prototype entries, including the "director" variant of Builder that the fluent interface replaces with a single caller-driven chain.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/13-builder-prototype`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
