# 08 - Strategy: shipping-rate estimation

A checkout needs to compute a delivery cost, and the carrier rate rule is
chosen at runtime: flat-rate, weight-based, or a distance-and-weight
tiered rate. This example prices the same `Shipment` three idiomatic C++
ways, side by side, so you can compare the mechanism each one uses to
select behavior without changing the pricing rules themselves.

## The domain

- `Shipment`: `weight_kg`, a `DestinationZone` (`Local`, `Regional`,
  `National`, `International`), and `declared_value_cents`.
- Money is tracked as integer `Cents`, never floating point, so a quote is
  exact and reproducible in a test assertion.
- The pricing math itself, `flat_rate_cost`, `weight_based_cost`, and
  `tiered_zone_cost` in `src/shipping_domain.cpp`, is shared by all three
  Strategy expressions below. Only the dispatch mechanism differs; an
  identical shipment prices identically no matter which one quoted it.

## The three Strategy expressions

1. **Runtime polymorphism** (`src/runtime_strategy.hpp`): an abstract
   `RateStrategy` interface with `FlatRateCarrier`, `WeightBasedCarrier`,
   and `TieredZoneCarrier` implementations. `RateEstimator` owns one
   carrier at a time as a `std::unique_ptr<RateStrategy>`, injected at
   construction and swappable later with `use_strategy()`.
2. **Value-semantic callable** (`src/function_strategy.hpp`): a strategy
   is just a `std::function<Cents(const Shipment&)>` (`RateFn`). `flat_rate`,
   `weight_based`, and `tiered_zone` are factory functions that return one.
   `QuoteBook` holds a `RateFn` by value and can be reassigned at runtime.
   `with_insurance_surcharge()` composes a new `RateFn` on top of an
   existing one: a class hierarchy would need a decorator class for that
   capability, but a callable gets it without extra code.
3. **Compile-time policy** (`src/policy_strategy.hpp`): a `RatePolicy`
   concept constrains a template parameter to anything with a
   `quote(const Shipment&) -> Cents` method. `FlatRatePolicy`,
   `WeightBasedPolicy`, and `TieredZonePolicy` satisfy it.
   `StaticRateCalculator<Policy>` picks its pricing behavior at compile
   time via the template argument: no virtual call, no type-erased
   callable, at the cost of no longer being swappable once the program is
   built.

### C++23 refinement (guarded)

`RateEstimator::try_estimate()` returns `std::expected<Cents, std::string>`
instead of throwing or returning a sentinel, rejecting a shipment with a
negative weight with a typed error. It is compiled only under C++23
(`#if __cplusplus >= 202302L` in `src/runtime_strategy.hpp` and
`.cpp`), so the mandatory C++20 build path never needs `std::expected`.
The matching gtest cases in `tests/runtime_strategy_test.cpp` are guarded
the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/08-strategy
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

Either path discovers the same 18 gtest cases under `cpp23` (16 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 18   # cpp23
100% tests passed, 0 tests failed out of 16   # cpp20
```

To see the three strategies price the same shipment side by side, build
and run the demo binary. From this directory, `./demo.sh` does the same
three steps above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target shipping_strategy_demo
./build/cpp23/shipping_strategy_demo
```

Expected output:

```
== Strategy 1: runtime polymorphism (unique_ptr injection) ==
Weight-based carrier: $10.25
Tiered-zone carrier (swapped at runtime): $12.30

== Strategy 2: std::function value-semantic callable ==
Flat-rate carrier: $9.99
Tiered-zone carrier + insurance (composed): $14.10

== Strategy 3: compile-time policy (concept-constrained template) ==
Weight-based policy: $10.25
Tiered-zone policy: $12.30

== C++23 refinement: typed validation via std::expected ==
Rejected malformed shipment: shipment weight_kg cannot be negative
```

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the compile-time policy section.

## Files

```
examples/08-strategy/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    shipping_domain.hpp/.cpp     # Shipment, Cents, shared pricing math
    runtime_strategy.hpp/.cpp    # expression 1: RateStrategy + RateEstimator
    function_strategy.hpp/.cpp   # expression 2: RateFn + QuoteBook
    policy_strategy.hpp          # expression 3: RatePolicy + StaticRateCalculator
    demo_main.cpp                 # exercises all three
  tests/
    runtime_strategy_test.cpp
    function_strategy_test.cpp
    policy_strategy_test.cpp
```
