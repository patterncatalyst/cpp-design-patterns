# 15 - Adapter, Facade, Bridge, and PImpl: a GlobalFreight integration

A checkout needs a shipping quote, a label, and a tracking number from a
carrier whose rate API predates this codebase and uses its own units and
names, a status change needs to notify a customer over whichever channel
they prefer, and one class's label formatting needs to stay out of its own
header. This example carries one GlobalFreight integration through all four
techniques so each one builds on the last.

## The domain

- `Shipment`: `weight_kg`, a `DestinationZone` (`Local`, `Regional`,
  `National`, `International`), and `declared_value_cents`.
- `LegacyGlobalFreightRateApi` (`src/legacy_rate_api.hpp`): the third-party
  rate lookup this example wraps. Weight travels in grams, the destination
  travels as a numeric zone code, the quote comes back as a plain `long`.

## Adapter: a legacy rate API behind RateSource

`RateSource` (`src/rate_adapter.hpp`) is the target interface: one pure
virtual `quote(const Shipment&) -> Cents`, with no mention of GlobalFreight
anywhere. `LegacyFreightRateAdapter` implements it by composing a
`LegacyGlobalFreightRateApi` (an object adapter, not a class adapter) and
translating kilograms to grams and `DestinationZone` to a zone code on
every call.

## Facade: one call fronts rating, labeling, and tracking

`ShippingFacade` (`src/shipping_facade.hpp`) owns a `RateSource`, a
`LabelRenderer`, and a `TrackingRegistry`, and exposes one method,
`quote_and_label()`, that runs all three in the order that keeps the
result consistent: quote first, then a tracking number, then a label that
prints that same tracking number. A caller never orchestrates the three
subsystems directly.

### C++23 refinement (guarded)

`try_quote_and_label()` returns `std::expected<ShippingQuote, std::string>`
instead of throwing on a malformed shipment (negative weight). It is
compiled only under C++23 (`#if __cplusplus >= 202302L` in
`src/shipping_facade.hpp` and `.cpp`), so the mandatory C++20 build path
never needs `std::expected`. The matching gtest cases in
`tests/shipping_facade_test.cpp` are guarded the same way.

## Bridge: notification kinds and transports vary independently

`Notification` (`src/notification.hpp`) holds a `NotificationTransport` by
pointer and delegates delivery to it. `ShipmentCreatedNotification` and
`DeliveryDelayedNotification` are the refined abstractions; `EmailTransport`
and `SmsTransport` are the implementations. Two kinds and two transports
produce four working combinations with no class per combination.

## PImpl: a label renderer with a compilation firewall

`LabelRenderer` (`src/label_renderer.hpp`) declares only its public
operations; `Impl`, defined entirely in `label_renderer.cpp`, holds every
formatting detail. The header hand-writes a destructor and a copy
constructor/assignment operator (required once `Impl` is an incomplete
type), and defaults the move operations, so the type still copies and
moves like an ordinary value.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/15-adapter-facade-bridge
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

Either path discovers the same 17 gtest cases under `cpp23` (15 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 17   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

To see all four techniques produce real output rather than just reading an
assertion that says they work, build and run the demo binary. From this
directory, `./demo.sh` does the same steps above plus runs it; or build
just the demo target after a `cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target adapt15_demo
./build/cpp23/adapt15_demo
```

Expected output:

```
== Adapter: the legacy GlobalFreight API behind RateSource ==
Adapted quote: 600 cents

== Facade: one call fronts rating, labeling, and tracking ==
price:    600 cents
label:    GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-1001
tracking: https://track.globalfreight.example/track/TRK-1001

== PImpl: LabelRenderer copies correctly, header hides its format ==
original: GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-9001
copy:     GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-9001

== Bridge: two notification kinds, two transports, no class explosion ==
EMAIL to customer@example.com: Your shipment TRK-1001 has been created.
SMS to +1-555-0100: Your shipment TRK-1001 has been created.
EMAIL to customer@example.com: Shipment TRK-1001 is delayed by 6 hours.
SMS to +1-555-0100: Shipment TRK-1001 is delayed by 6 hours.

== C++23 refinement: typed rejection via std::expected ==
Rejected malformed shipment: shipment weight_kg cannot be negative
```

The last block only prints under the `cpp23` preset; under `cpp20` the demo
stops after the Bridge section.

## Files

```
examples/15-adapter-facade-bridge/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py             # standalone Conan recipe (gtest/1.17.0)
  demo.sh                  # build + run the demo binary
  src/
    shipping_domain.hpp            # Shipment, DestinationZone, Cents
    legacy_rate_api.hpp/.cpp       # the legacy third-party rate API
    rate_adapter.hpp/.cpp          # RateSource + LegacyFreightRateAdapter
    label_renderer.hpp/.cpp        # PImpl label renderer
    shipping_facade.hpp/.cpp       # TrackingRegistry + ShippingFacade
    notification.hpp/.cpp          # Bridge: Notification + transports
    demo_main.cpp                   # exercises all four techniques
  tests/
    rate_adapter_test.cpp
    label_renderer_test.cpp
    shipping_facade_test.cpp
    notification_test.cpp
```
