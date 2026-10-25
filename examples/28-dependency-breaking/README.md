# 28 - The 24 dependency-breaking techniques for C++20/23

A representative subset of Michael Feathers' 24 dependency-breaking
techniques, applied to inherited shipping code. Each technique gets its
own small pair of inherited-dependency type plus the seam that makes it
substitutable, and a test that proves a double can stand in for the
original dependency.

## The domain

- `AlertChannel` / `SmtpAlertSender` (`src/alert_channel.hpp`) +
  `ShipmentAlertNotifier` (`src/shipment_alert_notifier.hpp`): **Extract
  Interface** (the mail dependency's minimal surface pulled out of the
  concrete `SmtpAlertSender`) combined with **Parameterize Constructor**
  (a defaulted second constructor parameter that preserves every existing
  call site while opening a seam for a test double).
- `ShipmentEtaScheduler` / `DeducingEtaScheduler`
  (`src/shipment_eta_scheduler.hpp`): **Extract and Override Call**. The
  wall-clock read is isolated into its own `current_time()` method,
  `virtual` in the C++20 form (a test subclass overrides it) and a
  deducing-this hook in the C++23 form, guarded behind
  `#if __cplusplus >= 202302L` (a derived class hides the base method by
  ordinary name lookup once `Self` deduces to it, no vtable required).
- `legacy_global` / `FuelSurchargeSource` / `LiveFuelSurchargeSource` /
  `ShipmentCostCalculator` (`src/legacy_fuel_surcharge.hpp`): **Encapsulate
  Global References**. The mutable global stays; pricing code now depends
  on an interface wrapping the read instead of calling the global inline.
- `LegacyCarrierZoneLookup` / `ZoneSurchargeReport` / `ZoneLookupPort` /
  `CarrierZoneLookupDelegator` / `ShipmentRoutingAdvisor`
  (`src/legacy_carrier_zone_lookup.hpp`): **Introduce Instance Delegator**.
  The static-only legacy lookup and its existing call site
  (`ZoneSurchargeReport`) are untouched; a thin instance delegator gives
  new code an injectable collaborator.
- `CarrierManifestFile` (`src/carrier_manifest_file.hpp`) +
  `ManifestSource` / `CarrierManifestFileAdapter` / `WarehouseDispatcher`
  (`src/manifest_source.hpp`): **Adapt Parameter**. The heavyweight,
  file-parsing legacy parameter type is adapted to a minimal interface, so
  `WarehouseDispatcher::dispatch()` takes the interface and a test never
  touches a filesystem.

## What each test proves

- `tests/shipment_alert_notifier_test.cpp`: the default constructor still
  builds a real `SmtpAlertSender` (existing call sites unchanged), a
  GoogleMock `MockAlertChannel` verifies the exact interaction, and a
  recording fake confirms the formatted message content.
- `tests/shipment_eta_scheduler_test.cpp`: the production path reads the
  real clock; a test subclass overrides `current_time()` for an exact,
  deterministic ETA; the C++23-only block repeats the proof through the
  deducing-this form.
- `tests/shipment_cost_calculator_test.cpp`: a calculator wired to a fixed
  `FuelSurchargeSource` keeps reporting the same total no matter how the
  legacy global changes after construction.
- `tests/shipment_routing_advisor_test.cpp`: the delegator forwards
  correctly to the static method, a mocked `ZoneLookupPort` drives new code
  with no legacy table involved, and the untouched legacy call site still
  works.
- `tests/warehouse_dispatcher_test.cpp`: a `FakeManifestSource` drives
  `WarehouseDispatcher::dispatch()` with no file on disk; one additional
  test builds a real `CarrierManifestFile` and its adapter to confirm the
  adapted path agrees with the fake path.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/28-dependency-breaking
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

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset, 13
under `cpp20`, the difference being the one deducing-this test that only
compiles under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 13   # cpp20
```

To see all six techniques run against production wiring (and, for the
mail notifier, a recording double) at the terminal, run the example's own
demo script:

```sh
examples/28-dependency-breaking/demo.sh cpp23
```

```
== Extract Interface + Parameterize Constructor: ShipmentAlertNotifier ==
  [smtp:mail.internal.example] to=ops-supervisor@example.com "Shipment SH-5001 flagged for manual review: weight exceeds declared value"
  recorded (no real SMTP call): to=ops-supervisor@example.com "Shipment SH-5002 flagged for manual review: recipient address flagged"

== Extract and Override Call: ShipmentEtaScheduler ==
  live schedule for SH-6001 computed against the real clock
  deducing-this variant (C++23) computed the same shape, no vtable involved

== Encapsulate Global References: ShipmentCostCalculator ==
  live total for a $100.00 base at the legacy global's current rate: $118.00

== Introduce Instance Delegator: ShipmentRoutingAdvisor ==
  local hub, zone 1
  legacy call site unchanged: zone 3 surcharge report for 99501

== Adapt Parameter: WarehouseDispatcher ==
  dispatched 275 shipments for FASTFREIGHT (large batch)
```

Running `examples/28-dependency-breaking/demo.sh cpp20` prints the same
output minus the deducing-this line, which only compiles under the
`cpp23` preset.

## Files

```
examples/28-dependency-breaking/
  CMakeLists.txt             # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json          # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py               # standalone Conan recipe (gtest/1.17.0)
  demo.sh                    # build + run the demo binary
  src/
    alert_channel.hpp/.cpp              # Extract Interface: AlertChannel + SmtpAlertSender
    shipment_alert_notifier.hpp/.cpp    # Parameterize Constructor
    shipment_eta_scheduler.hpp/.cpp     # Extract and Override Call (virtual + deducing-this)
    legacy_fuel_surcharge.hpp/.cpp      # Encapsulate Global References
    legacy_carrier_zone_lookup.hpp/.cpp # Introduce Instance Delegator
    carrier_manifest_file.hpp/.cpp      # Adapt Parameter: the heavyweight legacy type
    manifest_source.hpp/.cpp            # Adapt Parameter: the interface, adapter, and dispatcher
    demo_main.cpp                       # exercises all six techniques
  tests/
    shipment_alert_notifier_test.cpp
    shipment_eta_scheduler_test.cpp
    shipment_cost_calculator_test.cpp
    shipment_routing_advisor_test.cpp
    warehouse_dispatcher_test.cpp
```
