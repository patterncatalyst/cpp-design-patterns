# 13 - Builder, Prototype, and serialization: a shipment manifest

A dispatcher assembling a freight shipment manifest needs three required
fields (a shipment id, a carrier, a destination), a handful of optional
fields with sensible defaults (priority, insured value, handling notes),
and a list of packages that are usually small variations on a few known
kinds. This example builds that manifest with a **Builder**, fills its
package list by cloning from a **Prototype** registry, and round-trips the
finished manifest through a plain text **serialization** format.

## The domain

- `ShipmentManifest`: `shipment_id`, `carrier`, `destination` (required),
  `priority`, `insured_value_cents`, `special_handling_notes` (optional,
  defaulted), and a list of packages.
- Money is tracked as integer `Cents`, never floating point, the same
  convention `examples/08-strategy` uses, so a declared value is exact and
  reproducible in a test assertion.
- A `PackageTemplate` is a polymorphic package kind (`src/package_prototype.hpp`):
  `FragileElectronicsPackage`, `BulkDryGoodsPackage`, and
  `RefrigeratedPerishablePackage`, each with its own `handling_code` and a
  `clone()` that returns a deep, independent copy.

## Builder (`src/manifest_builder.hpp`)

`ManifestBuilder` assembles a `ShipmentManifest` through a fluent
interface: `shipment_id()`, `carrier()`, `destination()`, `priority()`,
`insured_value_cents()`, `special_handling_notes()`, and `add_package()`
each return `*this`, so calls chain. Optional fields already hold their
defaults (`Standard` priority, `0` insured cents, an empty notes string)
before a caller touches them. `build()` validates the whole manifest in
one place, required fields non-empty and at least one package, and throws
`std::invalid_argument` naming exactly what is missing; on success it
moves the assembled manifest out, leaving the builder ready to assemble
another one.

## Prototype (`src/package_prototype.hpp`)

`PackageTemplateRegistry` holds three named prototypes,
`"fragile-electronics"`, `"bulk-dry-goods"`, and
`"refrigerated-perishable"`, populated by
`default_package_template_registry()`. `create(name)` clones the named
prototype into a fresh instance; nothing about the registry's own copy
changes, and two clones of the same prototype are independent of each
other, mutating one never touches the other or the registry. `clone()`'s
deep copy costs nothing extra to write here: every concrete package's
state is a value type (a `double`, a `std::string`, a `Cents`), so the
compiler-generated copy constructor the base class exposes is already a
full, independent copy.

## Serialization (`src/manifest_serialization.hpp`)

`serialize_manifest()` writes a manifest as plain `KEY=VALUE` header lines
plus one `PACKAGE` line per package; `deserialize_manifest()` parses that
format back into a `ShipmentManifest`, reconstructing each package through
`make_package()` rather than through the registry, since a deserialized
package carries its own serialized weight and declared value rather than
a template's defaults. `manifests_equal()` in `src/manifest_domain.hpp`
compares two manifests field by field, including each package by `kind()`,
`weight_kg()`, `handling_code()`, and `declared_value_cents()`, so the
round trip can be checked for equality even though `ShipmentManifest`
itself is move-only.

### C++23 refinement (guarded)

`summary_line()` builds a one-line manifest summary with `std::format` and
compiles under both standards. `print_summary()` in
`src/manifest_serialization.hpp`/`.cpp` writes the same summary straight
to stdout with `std::print`, no intermediate `std::format` buffer, guarded
behind `#if __cplusplus >= 202302L` so the mandatory C++20 build path
never needs `<print>`. The matching gtest cases in
`tests/manifest_serialization_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/13-builder-prototype
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild, from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 19 gtest cases under `cpp23` (17 under
`cpp20`: the two `std::print` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 19   # cpp23
100% tests passed, 0 tests failed out of 17   # cpp20
```

To see the builder, the prototype registry, and the serialization round
trip run end to end, build and run the demo binary. From this directory,
`./demo.sh` does the same three steps above plus runs it; or build just
the demo target after a `cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target build13_manifest_demo
./build/cpp23/build13_manifest_demo
```

Expected output:

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

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after reporting the round trip result.

## Files

```
examples/13-builder-prototype/
  CMakeLists.txt        # standalone + superbuild-integrated targets (build13_ prefix)
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    manifest_domain.hpp/.cpp        # ShipmentManifest, Cents, ShippingPriority, manifests_equal
    package_prototype.hpp/.cpp      # Prototype: PackageTemplate + 3 kinds + registry
    manifest_builder.hpp/.cpp       # Builder: ManifestBuilder
    manifest_serialization.hpp/.cpp # serialize/deserialize + summary_line/print_summary
    demo_main.cpp                    # exercises builder, prototype, and the round trip
  tests/
    package_prototype_test.cpp
    manifest_builder_test.cpp
    manifest_serialization_test.cpp
```
