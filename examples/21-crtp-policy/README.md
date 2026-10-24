# 21 - CRTP and policy-based design: compile-time shipping quotes

A shipping-rate quote resolved with zero runtime dispatch: no vtable, no
type-erased `std::function`, nothing decided while the program is running
that the compiler could decide while it is building the program instead.
This example pairs two idiomatic C++ expressions of static polymorphism
side by side, so you can see what each one buys over the runtime
mechanisms Chapter 8 (Strategy) and Chapter 9 (Template Method) use to
select varying behavior.

## The domain

- `Shipment`: `weight_kg` and `declared_value_cents`, the same `Cents`
  integer-money convention every example in this book uses.
- Two unrelated idioms, not one pattern wearing two hats: CRTP resolves a
  derived type's method from a compile-time base; policy-based design
  composes a host class from independent, swappable policy types.

## CRTP: `RateModel<Derived>` (`src/rate_model.hpp`)

`RateModel<Derived>::quote()` casts `*this` down to `Derived` and calls
`Derived::compute()` directly, with no virtual function declared anywhere
in the base. `FlatRateModel` and `WeightedRateModel` each name themselves
as `RateModel`'s template argument, the construction every CRTP base
requires. A `static_assert` in the header proves the dispatch at compile
time: if `quote()` ever stopped resolving to the right `compute()`, the
header itself would fail to compile, not just a test.

`src/comparable_mixin.hpp` applies the same self-type trick to something
unrelated to rate models at all: `Comparable<Derived>` adds `operator<`,
`operator==`, and `operator>` to any `Derived` with a `sort_key()` method.
`src/quote_types.hpp` applies it to `ShipmentQuote` (orders by integer
`Cents`) and `CarrierRating` (orders by a floating-point score), two types
that share nothing except `Comparable` itself, proving the mixin is
general-purpose rather than specific to rate models.

## Policy-based design: `ShippingCalculator<Rounding, Surcharge>` (`src/shipping_calculator.hpp`)

Two independent policy families, each constrained by its own concept
(`RoundsCents`, `SurchargesShipment`), composed at compile time by
`ShippingCalculator`'s two template parameters:

- Rounding: `NoRoundingPolicy`, `RoundUpToNickelPolicy`.
- Surcharge: `NoSurchargePolicy`, `FlatSurchargePolicy`,
  `DeclaredValueSurchargePolicy` (1% of declared value).

Swapping either template argument changes `quote()`'s behavior with no
change to `ShippingCalculator` itself and no runtime indirection at the
call site. A `static_assert` proves the plainest composition, no rounding
and no surcharge, evaluates to exactly the base rate at compile time.

### C++23 refinement (guarded)

`src/rate_model_deducing_this.hpp` rewrites `RateModel` with C++23's
deducing `this`: `DeducedRateModel` is not a template, and `quote()` takes
an explicit object parameter instead of relying on a CRTP template
argument and a `static_cast`. It is compiled only under C++23
(`#if __cplusplus >= 202302L`), so the mandatory C++20 build path never
needs it. The matching gtest cases in `tests/deducing_this_test.cpp` are
guarded the same way, including one that asserts the CRTP and
deducing-`this` versions agree on the same shipment.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/21-crtp-policy
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

Either path discovers the same 13 gtest cases under `cpp23` (11 under
`cpp20`: the two deducing-`this` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 13   # cpp23
100% tests passed, 0 tests failed out of 11   # cpp20
```

To see the CRTP and policy-based quotes side by side, build and run the
demo binary. From this directory, `./demo.sh` does the same steps above
plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target crtp21_demo
./build/cpp23/crtp21_demo
```

Expected output:

```
== CRTP: RateModel<Derived> (static polymorphism) ==
Flat rate model: $9.99
Weighted rate model: $10.25

== CRTP mixin: Comparable<Derived> across unrelated types ==
Flat quote < weighted quote? true
Carrier A rated higher than carrier B? true

== Policy-based design: ShippingCalculator<Rounding, Surcharge> ==
Base rate, no rounding, no surcharge: $10.00
Base rate + flat surcharge, rounded to the nickel: $11.55
Base rate + 1% declared-value surcharge, rounded to the nickel: $11.25

== C++23 refinement: deducing this instead of CRTP ==
Deduced flat rate model (no template argument on the base): $9.99
```

The last line only prints under the `cpp23` preset; under `cpp20` the demo
stops after the policy-based design section.

## Files

```
examples/21-crtp-policy/
  CMakeLists.txt                     # standalone + superbuild-integrated targets
  CMakePresets.json                  # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                       # standalone Conan recipe (gtest/1.17.0)
  demo.sh                            # build + run the demo binary
  src/
    shipment_types.hpp               # Shipment, Cents
    rate_model.hpp                   # CRTP: RateModel<Derived> + FlatRateModel + WeightedRateModel
    comparable_mixin.hpp             # CRTP mixin: Comparable<Derived>
    quote_types.hpp                  # ShipmentQuote + CarrierRating, both using Comparable
    shipping_calculator.hpp          # policies + ShippingCalculator<Rounding, Surcharge>
    rate_model_deducing_this.hpp     # C++23 deducing-this rewrite of RateModel (guarded)
    demo_main.cpp                    # exercises all of the above
  tests/
    rate_model_test.cpp
    comparable_mixin_test.cpp
    shipping_calculator_test.cpp
    deducing_this_test.cpp           # C++23-only cases (guarded)
```
