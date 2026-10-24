---
layout: example
order: 8
title: "Strategy: shipping-rate estimation"
description: One shipment, three interchangeable rate strategies, priced identically and proven by GoogleTest.
github_path: examples/08-strategy
card_eyebrow: "Chapter 8 · Strategy"
card_title: "Shipping-rate estimation"
card_blurb: "A checkout picks a carrier rate rule at runtime. The same pattern is shown three idiomatic ways: a virtual interface, a std::function, and a compile-time concept policy."
---

A checkout needs to compute a delivery cost, and the rate rule depends on the
carrier. This demo models a small shipping domain (a `Shipment` with a weight,
a destination zone, and a declared value, priced in integer `Cents`) and shows
the Strategy pattern in three idiomatic C++ expressions, all pricing an
identical shipment identically:

1. **Runtime polymorphism.** An abstract `RateStrategy` with `FlatRateCarrier`,
   `WeightBasedCarrier`, and `TieredZoneCarrier`, injected into a
   `RateEstimator` through a `std::unique_ptr` and swapped at runtime.
2. **Value-semantic `std::function`.** A `RateFn` held by value, built by the
   `flat_rate` / `weight_based` / `tiered_zone` factories, with
   `with_insurance_surcharge()` composing a new strategy on top of an existing
   one at runtime.
3. **Compile-time policy.** A `RatePolicy` concept and a
   `StaticRateCalculator<Policy>` that selects the strategy at compile time with
   no indirection.

A C++23 refinement (`try_estimate()` returning `std::expected<Cents, std::string>`)
is included behind a feature guard, so the demo still builds under C++20.

## Build, run, observe

From the repo root, with the toolchain from the prerequisites chapter:

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 18/18 tests pass
./examples/08-strategy/demo.sh cpp23
```

The demo prints the computed costs for each expression (for example a
weight-based quote of `$10.25` and a tiered-zone quote of `$12.30`), shows a
runtime strategy swap, composes an insurance surcharge onto a `std::function`
strategy, and rejects a malformed shipment through the typed `std::expected`
path. Chapter 8 walks through how each expression works and when to reach for
which one.

The demo is also standalone-buildable from its own directory with
`conan install` and `cmake --preset cpp23`, and it builds under the `cpp20`
preset (16 tests, with the `std::expected` cases compiled out).
