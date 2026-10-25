# 26 - Anti-patterns and pattern misuse

A shipment-pricing calculation needs one number: a base cost multiplied by
a fuel surcharge, plus an optional handling fee. This example builds that
calculation two ways. The BEFORE version reaches it through a singleton
configuration registry, a singleton factory-of-factories, an interface
with exactly one implementation, and a god object that also validates,
logs, and formats, four misuses compounding into one tangled design. The
AFTER version reaches the identical answer through one small class
constructed with one plain value.

## The domain

- `ShippingRateRegistry` (`src/shipping_rate_registry.hpp`): BEFORE. A
  correct, thread-safe Meyers singleton holding one mutable field, the
  fuel surcharge multiplier, reachable and reassignable from anywhere in
  the process.
- `RateCalculator` / `StandardRateCalculator`
  (`src/rate_calculator.hpp`): BEFORE. An interface with exactly one
  implementation, which reads `ShippingRateRegistry::instance()` on every
  call rather than capturing a rate at construction time.
- `RateCalculatorFactory` / `StandardRateCalculatorFactory` /
  `RateCalculatorFactoryProvider` (`src/rate_calculator_factory.hpp`):
  BEFORE. A factory interface with one implementation, handed out by a
  second singleton, a "factory of factories" whose only job is returning
  the one factory this program will ever construct.
- `ShipmentOperationsManager` (`src/shipment_operations_manager.hpp`):
  BEFORE. A god object that resolves a calculator through both
  singletons, validates its input, logs every calculation, formats a
  receipt, and tracks whether a surcharge applied as mutable state on
  itself rather than on the receipt it returns, a leaky abstraction a
  caller can get wrong by holding an older receipt and asking the manager
  instead.
- `RateSettings` / `PricingResult` / `ShipmentPricingService`
  (`src/shipment_pricing_service.hpp`): AFTER. A plain value
  (`surcharge_multiplier`, `handling_fee`) injected once by constructor
  into one class, returning a value (`total_cost`, `surcharge_applied`)
  that carries everything a caller needs.

## What the BEFORE design costs

Five types and two singletons exist to multiply one number by another and
add a constant. None of the four misuses this chapter names is
hypothetical here:

- **Singleton as global mutable state.** `ShippingRateRegistry` has one
  slot for "the current surcharge." Two parts of the program that each
  believe they are configuring their own region are actually writing to
  the same slot.
- **Speculative generality.** `RateCalculator` and `RateCalculatorFactory`
  each have exactly one implementation and no second one anywhere in this
  codebase. Factory Method earns its cost when which concrete type to
  build is itself a decision; here it never varies.
- **God object.** `ShipmentOperationsManager` owns pricing orchestration,
  input validation, audit logging, and receipt formatting, four
  responsibilities with nothing in common except that each one, alone,
  seemed too small to deserve its own type.
- **Leaky abstraction.** `was_last_calculation_surcharge_applied()`
  answers a question about the manager's internal, call-order-dependent
  state, not about any specific receipt. A caller holding an earlier
  receipt and checking this method gets the wrong shipment's answer.

`tests/shipping_rate_registry_singleton_test.cpp` proves the first cost
directly: configuring a second region's surcharge retroactively changes
the total an already-obtained calculator for the first region reports,
even though nothing touched that calculator directly.
`tests/shipment_operations_manager_test.cpp` proves the fourth cost the
same way: pricing a second shipment changes what the surcharge flag
reports for a receipt returned by an earlier call.

## What the AFTER design proves

`tests/shipment_pricing_service_test.cpp` runs two independently
configured `ShipmentPricingService` instances, domestic and
international, in the same process, computes both, and confirms neither
one's result moves when the other is computed, the direct contrast with
the singleton test above. The same file substitutes a "fake" rate, which
for a value-shaped dependency is simply a different `RateSettings` value;
no interface, no mock framework, nothing to reset afterward.

`tests/equivalence_test.cpp` is the other half of the chapter's claim:
the BEFORE and AFTER designs compute identical totals for identical
inputs, and reject the same invalid input the same way. The difference
between the two designs is entirely in testability and in how many types
exist to express one calculation, never in the arithmetic.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/26-anti-patterns
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

Nothing in this chapter's code depends on a C++23-only feature, so both
presets discover the identical set of gtest cases via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 10   # cpp23
100% tests passed, 0 tests failed out of 10   # cpp20
```

To see the BEFORE design's spooky action at a distance and the AFTER
design's isolation run side by side, build and run the demo binary. From
this directory, `./demo.sh` does the same steps above plus runs it:

```sh
examples/26-anti-patterns/demo.sh cpp23
```

Expected output:

```
== Over-patterned (BEFORE): singleton + factory-of-factories ==
Region A quote (surcharge 1.10): $110.00
ORD-3001 priced via the manager: $110.00
ORD-3002 priced via the manager: $130.00
Region A quote, re-read with no code touching it: $130.00
(the surcharge moved out from under region A; nothing about
 region_a_calculator was ever reassigned)
was_last_calculation_surcharge_applied() after ORD-3002: true -- answers for ORD-3002, not for the ORD-3001 receipt still held above

== Simplified (AFTER): plain dependency injection of RateSettings ==
Domestic total (surcharge 1.00): $102.50
Domestic surcharge applied: false
International total (surcharge 1.35): $144.00
International surcharge applied: true
Domestic total, re-read after pricing international: $102.50
(unchanged: domestic and international each hold their own
 RateSettings value, so pricing one never moves the other)
```

Region A's quote changes from $110.00 to $130.00 with no code touching
`region_a_calculator` directly; the domestic total stays at $102.50 no
matter what the international pricing call does. That contrast is the
entire argument this chapter makes, visible at the terminal rather than
asserted in prose. Running `examples/26-anti-patterns/demo.sh cpp20`
prints identical output; nothing here is standard-dependent.

## Files

```
examples/26-anti-patterns/
  CMakeLists.txt             # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json          # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py               # standalone Conan recipe (gtest/1.17.0)
  demo.sh                    # build + run the demo binary
  src/
    shipping_rate_registry.hpp/.cpp         # BEFORE: singleton, global mutable surcharge
    rate_calculator.hpp/.cpp                # BEFORE: interface with one implementation
    rate_calculator_factory.hpp/.cpp        # BEFORE: factory-of-factories, one impl each
    shipment_operations_manager.hpp/.cpp    # BEFORE: god object + leaky surcharge flag
    shipment_pricing_service.hpp/.cpp       # AFTER: RateSettings value + ShipmentPricingService
    demo_main.cpp                           # exercises both designs side by side
  tests/
    equivalence_test.cpp                       # BEFORE and AFTER compute identical totals
    shipping_rate_registry_singleton_test.cpp  # proves global-state spooky action at a distance
    shipment_operations_manager_test.cpp       # proves the leaky surcharge flag + god-object logging
    shipment_pricing_service_test.cpp          # proves isolation + fake substitution
```
