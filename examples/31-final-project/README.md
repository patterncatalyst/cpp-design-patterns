# 31 - Final project

A shipment dispatch console composing five patterns into one coherent
system: Strategy prices a shipment, Factory builds the carrier that backs
that Strategy, State enforces the shipment's lifecycle, Command wraps every
operator action with an undo, and Observer fans each lifecycle change out
to whoever is watching. `ShipmentDispatcher` is the composition root: the
one place in this example that knows about all five at once.

## The domain

- `Shipment` (`src/shipment.hpp`): the State component's context and the
  Observer component's subject in one type. `advance()` is the only legal
  way its status changes, rejecting an event that does not apply to the
  current status with `IllegalTransitionError` and notifying every
  subscribed `LifecycleObserver` only after a legal transition succeeds.
  `restore_status_for_undo()` is the seam Command's undo path uses instead.
- `RateStrategy` / `FlatRateStrategy` / `WeightTieredRateStrategy`
  (`src/rate_strategy.hpp`, `src/flat_rate_strategy.hpp`,
  `src/weight_tiered_rate_strategy.hpp`): the Strategy component. A
  shipment's cost is computed by whichever concrete strategy its carrier's
  contract calls for.
- `CarrierFactory` (`src/carrier_factory.hpp`): the Factory component.
  `create()` turns a `CarrierRequest`'s `carrier_code` into a
  `CarrierProfile`, a carrier name, an SLA, and the matching `RateStrategy`,
  all three built from one call so they can never disagree with each
  other. Under C++23, `try_create()` returns the same result as
  `std::expected<CarrierProfile, CarrierFactoryError>` instead of throwing.
- `ShipmentCommand` / `AdvanceLifecycleCommand` / `AdjustWeightCommand`
  (`src/shipment_command.hpp` and friends): the Command component.
  `AdvanceLifecycleCommand` wraps a State transition; `AdjustWeightCommand`
  recomputes a shipment's quote through its `RateStrategy` on execute and
  restores an exact snapshot on undo. `CommandHistory` (`src/command_history.hpp`)
  is the undo stack both run through.
- `LifecycleObserver` (`src/lifecycle_observer.hpp`): the Observer
  component's interface. `Shipment` fans every transition, legal or
  undo-restored, out to each subscriber.
- `ShipmentDispatcher` (`src/shipment_dispatcher.hpp`): the composition
  root. `open_shipment()` calls the factory and prices the opening quote;
  `make_advance_command()` and `make_adjust_weight_command()` wire a
  command to this dispatcher's shipment and, where relevant, its
  factory-built strategy; `apply()` and `undo_last()` run commands through
  the history; `subscribe()` registers an observer on the shipment.

## What the tests prove

Six test files back this example. The first five each cover one
component's own contract in isolation:
`tests/shipment_lifecycle_test.cpp` (State + Observer notification),
`tests/rate_strategy_test.cpp` (Strategy), `tests/carrier_factory_test.cpp`
(Factory), `tests/adjust_weight_command_test.cpp` and
`tests/advance_lifecycle_command_test.cpp` (Command), and
`tests/command_history_test.cpp` (the undo stack, driven with a GoogleMock
command double).

`tests/shipment_dispatcher_integration_test.cpp` is this example's actual
point. It never repeats a single-pattern check; it proves the patterns
work together through `ShipmentDispatcher`:

- `FactoryBuiltStrategyPricesTheOpeningQuote` confirms the quote
  `open_shipment()` computes is whatever the factory-built `RateStrategy`
  returns for the requested carrier, not a value the dispatcher invented.
- `LifecycleCommandsFireObserverEventsInOrderAsStatusAdvances` subscribes a
  `MockLifecycleObserver` with `InSequence` and drives two
  `AdvanceLifecycleCommand`s through the dispatcher, proving a legal State
  transition triggered by a Command fires the Observer notification in
  order.
- `WeightCorrectionRecomputesViaStrategyAndUndoRestoresBoth` proves a
  Command invokes the Strategy on execute and restores the exact prior
  weight and quote on undo, with no second strategy call needed.
- `UndoingALifecycleCommandRestoresStatusAndFiresACorrectionEvent` proves a
  Command's undo of a State transition still notifies the Observer.
- `FullRunComposesAllFivePatternsEndToEnd` runs the whole sequence, a
  factory-priced opening quote, two lifecycle transitions with their
  notifications, a weight correction, and an undo, in a single assertion
  chain.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/31-final-project
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

`CarrierFactory::try_create()` only exists under C++23, so the two presets
discover a different test count. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 34   # cpp23
100% tests passed, 0 tests failed out of 32   # cpp20
```

To see the composed system run end to end, not just the tests that back
it, run the example's own demo script:

```sh
examples/31-final-project/demo.sh cpp23
```

Expected output:

```
== Opening a shipment with Skyhaul Express ==
  carrier: Skyhaul Express (SLA 24h)
  opening quote at 8kg: $27.20

== Advancing the lifecycle: PickUp, Dispatch ==
  [observer] SHIP-4821: Created -> PickedUp
  [observer] SHIP-4821: PickedUp -> InTransit
  status now: InTransit

== A warehouse rescale corrects the weight to 15kg ==
  weight now: 15kg, recomputed quote: $37.00

== The rescale turns out to be a scanner glitch; undo it ==
  weight restored to: 8kg, quote restored to: $27.20

== Delivering the shipment ==
  [observer] SHIP-4821: InTransit -> Delivered
  status now: Delivered

== Operator mis-scanned PickUp again by habit; the console rejects it ==
  rejected: illegal transition: event 'PickUp' does not apply to status 'Delivered'
```

Running `examples/31-final-project/demo.sh cpp20` prints identical output;
nothing in the demo path depends on a C++23-only feature.

## Files

```
examples/31-final-project/
  CMakeLists.txt                       # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json                    # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                         # standalone Conan recipe (gtest/1.17.0)
  demo.sh                              # build + run the dispatch console demo
  src/
    shipment_types.hpp/.cpp                  # ShipmentStatus, ShipmentEvent, IllegalTransitionError
    lifecycle_observer.hpp                   # Observer interface
    shipment.hpp/.cpp                        # State context + Observer subject
    rate_strategy.hpp                        # Strategy interface
    flat_rate_strategy.hpp/.cpp              # Strategy: flat fee
    weight_tiered_rate_strategy.hpp/.cpp     # Strategy: weight-bracket pricing
    carrier_factory.hpp/.cpp                 # Factory: builds a CarrierProfile per carrier code
    shipment_command.hpp                     # Command interface
    advance_lifecycle_command.hpp/.cpp       # Command meets State
    adjust_weight_command.hpp/.cpp           # Command meets Strategy
    command_history.hpp/.cpp                 # the undo stack
    shipment_dispatcher.hpp/.cpp             # the composition root
    demo_main.cpp                            # plays back one shipment end to end
  tests/
    shipment_lifecycle_test.cpp              # State + Observer
    rate_strategy_test.cpp                   # Strategy
    carrier_factory_test.cpp                 # Factory
    adjust_weight_command_test.cpp           # Command + Strategy
    advance_lifecycle_command_test.cpp       # Command + State + Observer
    command_history_test.cpp                 # the undo stack, with a mock command
    shipment_dispatcher_integration_test.cpp # the patterns composed together
```
