---
title: "Builder, Prototype, and serialization"
order: 13
part: "creating-objects-under-control"
description: "A fluent builder that assembles a multi-field shipment manifest and validates it only once, at build(), a small named registry of package prototypes that clone into fresh independent copies, and a plain-text serialization format that round-trips a manifest back to an equal value, taught against a freight dispatch domain."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force Builder answers: a product has several required fields, several optional ones with sensible defaults, and an invariant that only holds once every field is in place, and no single constructor signature expresses all three cleanly.
- Build a fluent `ManifestBuilder` whose chained setters return `*this`, and explain why validation belongs only at `build()`, not scattered across the individual setters.
- Name the force Prototype answers: a handful of pre-configured object kinds get reused constantly, and reconstructing one from scratch every time repeats the same initialization logic for no benefit.
- Build a `PackageTemplateRegistry` that clones a named prototype into a fresh, independently owned copy, and say why a deep copy falls out of the compiler-generated copy constructor here with no hand-written cloning logic underneath it.
- Round-trip a manifest through a plain-text serialization format and verify the result with a structural equality check, not a byte-for-byte string comparison.
- Recognize when a Builder or a Prototype is solving a problem a plain constructor or a factory function already solves, and say what to write instead.

## Why this matters

A freight dispatcher assembling a shipment manifest is filling in three different kinds of field at once. Some are non-negotiable: a shipment without a carrier or a destination is not a shipment, it is a typo waiting to be caught. Some are optional with a default a dispatcher rarely overrides: most shipments ship at standard priority with no declared insurance, and typing that out for every manifest would be noise, not information. One more constraint holds across the whole manifest regardless of which fields a caller set: a manifest with zero packages in it is not valid, no matter how carefully the header fields were filled in.

A single constructor cannot express all three of those forces at once without becoming unreadable. A constructor that takes every field, required and optional, forces every caller to pass values for fields they do not care about, in a parameter order nobody will remember six months from now. A constructor overload for every combination of optional fields a caller might want to set is combinatorial, and it grows every time this book's example domain adds one more optional field. Builder is the Gang of Four's name for resolving this: a separate object accumulates field values one call at a time through a fluent interface, applies the defaults for whatever the caller skipped, and checks the one cross-field invariant exactly once, at the single moment the product is produced.

A related but distinct force shows up on the other side of the same manifest: its packages. A freight operation does not invent a new package shape for every order. A handful of kinds, fragile electronics, bulk dry goods, refrigerated perishables, come up constantly, each with its own default weight and handling code. Writing out a `FragileElectronicsPackage` constructor call by hand at every call site duplicates the same defaults dozens of times over, and a change to one default has to be hunted down everywhere it was copied. Prototype is the Gang of Four's name for resolving this instead: keep one configured example of each kind, a prototype, and get a new instance by cloning it rather than by re-deriving its defaults from scratch.

The third piece this chapter adds, serialization, is not a Gang of Four pattern on its own, but it is the natural companion to both. Once a manifest exists as an in-memory object graph, something downstream almost always needs it as text: a log line, a message on a queue, a file a trading partner's system reads later. Writing that text out and parsing it back is where a design's claims about equality are checked against a concrete value, not a read of the source code. If `serialize_manifest()` and `deserialize_manifest()` round-trip a manifest and the result is not equal to the one that produced it, something in the Builder's defaults or the Prototype's cloning was incomplete, and the round trip is what catches it, not a visual read of the text.

{% include excalidraw.html name="13-builder-prototype" caption="Figure 13.1: ManifestBuilder assembles a ShipmentManifest behind a fluent interface and validates it once at build(); PackageTemplateRegistry clones a named prototype into an independent copy; serialize_manifest() and deserialize_manifest() round-trip the result through plain text." %}

## The domain: assembling a freight shipment manifest

The example behind this chapter lives in `examples/13-builder-prototype`. A `ShipmentManifest` carries three required header fields, three optional ones already holding their defaults, and a list of packages:

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

Money is tracked as integer `Cents`, the same convention Chapter 8's shipping-rate example uses, so a declared value compares exactly in a test assertion with no floating-point epsilon anywhere. The `packages` field is a vector of owning pointers to a polymorphic `PackageTemplate` rather than a vector of values, because each package kind, fragile electronics, bulk dry goods, refrigerated perishable, carries its own `clone()` behavior, and that is exactly the capability Prototype needs. The consequence is that `ShipmentManifest` itself is move-only: it has no copy constructor, because copying a `unique_ptr` is not defined. This is the correct shape for an object that is produced once, by a builder or by `deserialize_manifest()`, and handed off by value afterward.

## Builder: assembling the manifest through a fluent interface

`ManifestBuilder` holds one `ShipmentManifest` internally and exposes a chained setter for every field:

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

Every setter returns `*this` by reference, which is what lets a caller chain calls in whatever order is convenient rather than filling in a fixed parameter list:

```cpp
ShipmentManifest built = ManifestBuilder()
    .shipment_id("SHIP-4471")
    .carrier("Northline Freight")
    .destination("Columbus, OH")
    .priority(ShippingPriority::Expedited)
    .add_package(registry.create("fragile-electronics"))
    .build();
```

Nothing is validated as the chain runs. `priority()` does not check that `shipment_id()` was called first, and `add_package()` does not check that the manifest already has a carrier. The validation step belongs only at `build()`, not scattered across the setters, because a half-built manifest is not finished yet, not an invalid one. `build()` is the single moment a caller declares the manifest complete:

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

Checking every required field in one function, run exactly once, means the invariant "a manifest leaving `build()` is always complete" has exactly one place it could fail to hold, and the test suite only has to exercise that one function to prove the invariant. `build()` also moves the manifest out with `std::exchange`, replacing the builder's internal state with a fresh, empty `ShipmentManifest` in the same statement, so a builder is reusable for a second manifest immediately after the first one is built, with no leftover package from the first order carried into the second.

## Prototype: cloning a pre-configured package

A `PackageTemplate` is the base class every concrete package kind implements, and `clone()` is its one Prototype-specific responsibility:

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

`FragileElectronicsPackage::clone()` is one line: `return std::make_unique<FragileElectronicsPackage>(*this);`. That one line is already a full, independent deep copy, and the reason comes directly from how the class is built rather than from anything Prototype guarantees on its own. Every member `PackageTemplate` and its subclasses declare is a value type: a `double`, a `std::string`, a `Cents`. None of them is an owned pointer this class would otherwise have to clone by hand, so the compiler-generated copy constructor, exposed through the protected `PackageTemplate(const PackageTemplate&) = default` above, already copies every byte of state a new instance needs. If a future package kind ever held a `unique_ptr` member of its own, say a pointer to a large attached document, `clone()` would need a hand-written copy of that member too; the deep copy here is a property of this class's data layout, not a guarantee Prototype provides for every class automatically.

`PackageTemplateRegistry` is the small, named catalogue a dispatcher clones from:

```cpp
class PackageTemplateRegistry {
public:
    void register_template(std::string name, std::unique_ptr<PackageTemplate> prototype);
    std::unique_ptr<PackageTemplate> create(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<PackageTemplate>> prototypes_;
};
```

`create("fragile-electronics")` calls `clone()` on the registered prototype and hands the caller the result. The registry's own copy of that prototype is never handed out and never mutated; every call to `create()` with the same name returns a new, independent object. `examples/13-builder-prototype`'s test suite checks exactly that claim directly: clone twice from the same name, mutate one clone's `weight_kg`, and assert the other clone and a third fresh clone both still report the prototype's original default. If `clone()` ever returned a shared pointer to the same underlying package instead of a fresh copy, that test would fail the moment the first clone was mutated, which is the point of writing the assertion against the mutation rather than against the clone's initial state alone.

## Serialization: proving the round trip, not just reading it

`serialize_manifest()` writes a manifest as plain text, one `KEY=VALUE` line per header field and one `PACKAGE` line per package:

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

`deserialize_manifest()` parses that format back, reconstructing every package through `make_package()`, a small factory function keyed on the `kind` string, rather than through `PackageTemplateRegistry`. That distinction matters: a deserialized package carries its own serialized weight and declared value, which might differ from any registered prototype's defaults once a dispatcher has adjusted them, so reconstruction has to build the concrete type directly from the parsed fields rather than cloning a registry entry and hoping the defaults still match.

The test that proves the round trip works cannot compare two `ShipmentManifest` values with `==`, because `ShipmentManifest` is move-only and holds no such operator. `manifests_equal()` compares the two manifests structurally instead: every header field directly, and every package pairwise by `kind()`, `weight_kg()`, `handling_code()`, and `declared_value_cents()`, the same four accessors `PackageTemplate` already exposes publicly for exactly this purpose:

```cpp
TEST(ManifestSerializationTest, RoundTripReproducesAnEqualManifestWithMultiplePackages) {
    const ShipmentManifest original = sample_manifest();
    const std::string serialized = serialize_manifest(original);
    const ShipmentManifest round_tripped = deserialize_manifest(serialized);

    EXPECT_TRUE(manifests_equal(original, round_tripped));
}
```

This is the clone-versus-construct distinction stated as a test rather than as a design note. Cloning, through `PackageTemplate::clone()`, reproduces a package from an existing in-memory object the program already holds a pointer to. Constructing from serialized text, through `make_package()`, reproduces an equivalent package from a flat string with no relationship to any particular in-memory object at all. Both paths have to agree on what "equivalent" means, kind, weight, handling code, declared value, or the round trip and the clone check would drift apart from each other over time as the domain grows a new field. Writing `manifests_equal()` once and using it in both places keeps that definition in exactly one function.

## A narrower contrast: std::format versus std::print

Everything above builds a `std::string` before anything is written anywhere, the same convention `summary_line()` uses for a one-line manifest summary:

```cpp
std::string summary_line(const ShipmentManifest& manifest_obj) {
    return std::format("{} via {} to {} [{}], {} package(s), insured for ${}.{:02d}",
                        manifest_obj.shipment_id, manifest_obj.carrier, manifest_obj.destination,
                        to_string(manifest_obj.priority), manifest_obj.packages.size(),
                        manifest_obj.insured_value_cents / 100, manifest_obj.insured_value_cents % 100);
}
```

A caller that wants the summary as a `std::string`, to embed in a log line or compare in a test, needs exactly this. A caller that only ever wants the summary written to the terminal is paying for an intermediate allocation it never uses: `summary_line()` builds a `std::string`, and the caller then has to hand that string to `std::cout` separately. C++23's `std::print` (and `std::println`, which appends the trailing newline automatically) collapses both steps into one call that writes straight to `stdout`, with no intermediate buffer a caller has to manage:

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

`print_summary()` is guarded behind `#if __cplusplus >= 202302L` in `src/manifest_serialization.hpp`, so the mandatory C++20 build path never requires `<print>`. Neither Builder nor Prototype changes shape between the two standards; the contrast here is narrower than either pattern, a single formatting call site choosing between a reusable string and a direct write, the same kind of narrow, single-call-site illustration Chapter 9 used for CRTP versus deducing this.

> **AI sidebar.** Ask an LLM pairing partner for a Builder and a common result validates inside every setter instead of once at `build()`, because "fail fast" sounds like good advice in isolation. Read that result the way this chapter trained: if `carrier()` throws whenever `shipment_id()` has not been called yet, the builder has reintroduced the fixed-parameter-order problem Builder exists to remove, just spread across several methods instead of one constructor. The same generated code often skips a Prototype's `clone()` entirely and returns a `shared_ptr` to the same underlying object from every "clone" call, which looks correct in a demo that never mutates the result and breaks the instant two callers do. Check specifically for that: does mutating one cloned instance leave every other clone, and the registered prototype itself, unchanged? A generated registry with no test exercising that question has not demonstrated it clones at all, only that it compiles.

## When not to use Builder or Prototype

Builder earns its fluent interface when a product has enough required and optional fields, with at least one cross-field invariant, that a single constructor signature would be unreadable or would force every caller to repeat the same defaults. It does not earn that interface below that bar. A `ShipmentManifest` with two required fields and no optional ones does not need `ManifestBuilder`; it needs a constructor, or an aggregate initializer, the same candidate-non-pattern move this book has trained since Chapter 1. Introducing a builder ahead of that complexity adds a class, a chain of methods a reader has to trace back to a single `build()` call, and a runtime validation check standing in for what the type system could have enforced directly with required constructor parameters.

Prototype earns its registry when a handful of pre-configured kinds get reused often enough that re-deriving their defaults at every call site is real, visible duplication, and when those kinds are closed enough in number that a small named catalogue pays for its own upkeep. It does not earn a registry when there is exactly one kind of package, or when every order's package differs enough from every other that there is nothing left to clone. A single `Package` struct with no polymorphism and no registry, filled in directly at each call site, serves that case better than a `PackageTemplate` hierarchy with one concrete subclass and nothing to clone from. The test is the one this book keeps returning to: if cloning saves real, repeated initialization work, Prototype is the right name; if there is nothing common to clone, a plain constructor is the stronger engineering choice even with Prototype's name sitting right there.

A related trap is reaching for Builder when what is needed instead is several small, independent factory functions. If a caller never needs to set more than two or three fields at once and there is no shared cross-field invariant to check, a set of free functions, each returning a complete, valid object, is plainer to read than a fluent chain that only ever gets used two calls deep. Builder's value is concentrated in the validation step at `build()`; a design with no meaningful validation to centralize has already given most of that value away before paying any of Builder's cost.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/13-builder-prototype` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 19 cases under the `cpp23` preset, 17 under `cpp20`, the difference being the two `std::print`-based cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 19   # cpp23
100% tests passed, 0 tests failed out of 17   # cpp20
```

To see the builder, the prototype registry, and the serialization round trip run end to end rather than just reading an assertion that says each one is correct, run the example's own demo script:

```sh
examples/13-builder-prototype/demo.sh cpp23
```

It assembles a manifest, rejects an incomplete one, clones two independent packages from the same prototype, round-trips the manifest through plain text, and confirms the round trip reproduced an equal manifest:

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

Notice the second clone still reports 14.75 kg after the first clone was mutated to 9.25 kg: that is `PackageTemplateRegistry::create()`'s independence guarantee showing up as an observable fact at the terminal, not just as a passing test. Running `examples/13-builder-prototype/demo.sh cpp20` prints the same output minus the final `std::print` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Builder and Prototype entries, including the "director" variant of Builder this chapter's fluent interface simplifies away in favor of a single caller-driven chain.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/13-builder-prototype`'s suite, which asserts structural equality across a serialization round trip and independence across a prototype clone rather than only checking that the binaries exit zero.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
