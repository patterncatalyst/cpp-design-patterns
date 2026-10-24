# 12 - Factory Method and Abstract Factory: carrier integration

A checkout has to process a shipment through whichever carrier a merchant
has under contract, and processing means producing three related things in
a format that one specific carrier expects: a shipping label, a rate
quote, and a tracking URL. This example shows two idiomatic C++ ways to
keep the choice of carrier out of the client code that uses these objects.

## The domain

- `Shipment`: `destination_country`, `weight_kg`, `declared_value_cents`.
- `CarrierAccount`: `carrier_name`, `account_id`, `webhook_registered`, a
  merchant's account with one carrier, used by the Factory Method half of
  this example.
- Two carriers, **FastShip** (flat base fee plus per-kilogram) and
  **SummitFreight** (per-kilogram plus a declared-value tier), each
  implementing the same three product roles differently.

## Abstract Factory: a matching family per carrier

`CarrierIntegrationFactory` (`src/abstract_factory.hpp`) declares three
creation methods, one per abstract product: `LabelFormatter`, `RateQuoter`,
`TrackingClient`. `FastShipIntegrationFactory` and
`SummitFreightIntegrationFactory` each implement all three, every one
tagged with that carrier's own label format, rate formula, and tracking
domain.

`ShipmentProcessor` is the client: its constructor takes one
`CarrierIntegrationFactory` and builds all three products from it in that
one call. After construction there is no way for a FastShip label
formatter to end up paired with a SummitFreight rate quoter; the family
came from one factory and stays together for the processor's lifetime.
`ShipmentProcessor` itself never names `FastShipIntegrationFactory` or
`SummitFreightIntegrationFactory` anywhere in its own code, only the three
abstract product interfaces.

`make_carrier_factory()` looks a factory up by carrier name, the call a
merchant's checkout makes once per order after reading the chosen carrier
out of configuration.

### C++23 refinement (guarded)

`try_make_carrier_factory()` returns
`std::expected<std::unique_ptr<CarrierIntegrationFactory>, std::string>`
instead of throwing on an unrecognized carrier name. It is compiled only
under C++23 (`#if __cplusplus >= 202302L` in `src/abstract_factory.hpp`
and `.cpp`), so the mandatory C++20 build path never needs
`std::expected`. The matching gtest cases in `tests/abstract_factory_test.cpp`
are guarded the same way.

## Factory Method: one product, decided by environment

`CarrierOnboarder` (`src/factory_method.hpp`) owns a fixed onboarding
sequence in `onboard()`: validate the account id, run whichever
`IntegrationTestSuite` the factory method returns, judge the outcome.
`create_test_suite()` is the one private virtual method, the factory
method itself, that `SandboxOnboarder` and `ProductionOnboarder` override
to decide which concrete suite runs: `SandboxTestSuite` skips the webhook
delivery check, `ProductionSmokeTestSuite` requires it. Neither subclass
touches `onboard()`'s sequence; both inherit it unchanged.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/12-factories
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

Either path discovers the same 14 gtest cases under `cpp23` (12 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`.

To see both patterns in action rather than just reading an assertion that
says they work, build and run the demo binary. From this directory,
`./demo.sh` does the same steps above plus runs it; or build just the demo
target after a `cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target factory12_demo
./build/cpp23/factory12_demo
```

## Files

```
examples/12-factories/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py             # standalone Conan recipe (gtest/1.17.0)
  demo.sh                  # build + run the demo binary
  src/
    shipping_domain.hpp            # Shipment, CarrierAccount, Cents
    abstract_factory.hpp/.cpp      # CarrierIntegrationFactory + two carriers + ShipmentProcessor
    factory_method.hpp/.cpp        # CarrierOnboarder + Sandbox/Production onboarders
    demo_main.cpp                   # exercises both patterns
  tests/
    abstract_factory_test.cpp
    factory_method_test.cpp
```
