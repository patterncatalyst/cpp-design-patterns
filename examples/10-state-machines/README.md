# 10 - State and finite state machines: a shipment lifecycle

A shipment moves through a small, fixed set of statuses from checkout to
delivery: `Created`, `Paid`, `InTransit`, `Delivered`, with `Cancelled`
reachable from `Created` or `Paid`. This example models that lifecycle four
idiomatic C++ ways, side by side, so you can compare the mechanism each one
uses to encode "which events are legal right now" without changing the
lifecycle itself.

## The domain

- `ShipmentStatus`: `Created`, `Paid`, `InTransit`, `Delivered`,
  `Cancelled`, and `Lost`. The first five are the legal lifecycle every
  expression below can reach; `Lost` exists only as an outcome the
  stochastic machine can draw.
- `ShipmentEvent`: `Pay`, `Ship`, `Deliver`, `Cancel`, and `Advance` (the
  stochastic machine's own weighted step, named for error messages only).
- `IllegalTransition`, declared in `src/shipment_domain.hpp`, is thrown by
  every expression below the same way when an event does not apply to the
  current status.
- Unlike Chapter 8's three Strategy expressions, which shared one pricing
  function, the three deterministic machines below each encode the legal
  transitions independently, in their own idiom. The test suite proves
  they agree; it does not rely on them sharing code to agree.

## The four expressions

1. **GoF State pattern** (`src/gof_state.hpp`): a `ShipmentState`
   interface with one concrete class per status (`CreatedState`,
   `PaidState`, `InTransitState`, `DeliveredState`, `CancelledState`).
   Every event defaults to rejecting the call; a concrete state overrides
   only the events it accepts. `GofStateMachine` owns the current state as
   a `std::unique_ptr` and replaces it with whatever the accepted event
   returns.
2. **Data-driven FSM** (`src/table_fsm.hpp`): every legal transition is one
   row, `(from, event) -> to`, in a literal `std::unordered_map`.
   `TableShipmentMachine::apply()` looks up the row and throws
   `IllegalTransition` if none matches; there is no per-status branching
   code anywhere in this expression.
3. **`std::variant` FSM** (`src/variant_fsm.hpp`): each status is an empty
   tag type (`CreatedTag`, `PaidTag`, ...), and `ShipmentVariant` is a
   `std::variant` over all five. `apply_event()` pattern-matches the active
   alternative with `std::visit` and an overloaded-lambda set, returning a
   new variant holding the next state.
4. **Stochastic FSM** (`src/stochastic_fsm.hpp`): `pay()`, `ship()`, and
   `cancel()` enforce the same deterministic rules as the table FSM.
   `advance()`, callable only while `InTransit`, draws a weighted outcome
   (0.85 `Delivered`, 0.10 delayed/stays `InTransit`, 0.05 `Lost`) from a
   `std::mt19937` engine injected through the constructor.
   `run_to_terminal()` calls `advance()` in a loop until the shipment
   reaches `Delivered` or `Lost`.

### Making the stochastic machine deterministic for tests

`StochasticShipmentMachine` takes its `std::mt19937` engine as a
constructor parameter rather than owning one internally or seeding from
`std::random_device` itself. A production caller passes
`std::mt19937{std::random_device{}()}` for a real, unpredictable seed;
`tests/stochastic_fsm_test.cpp` passes `std::mt19937(38)`, a fixed seed
chosen because it produces a short, illustrative run (three delays before
delivery) under this repository's toolchain. Seeding the same engine with
the same value always produces the same sequence of draws, which is what
lets `StochasticShipmentMachineTest.SeededRunProducesAnExactReproducibleSequence`
assert an exact, hardcoded sequence instead of merely checking that the
machine eventually terminates. A separate test runs 20,000 independent
single-step trials from freshly seeded machines and checks that the
empirical outcome frequencies land within 2 percentage points of the
configured 0.85 / 0.10 / 0.05 weights.

### C++23 refinement (guarded)

`TableShipmentMachine::try_apply()` returns
`std::expected<ShipmentStatus, std::string>` instead of throwing, reporting
a rejected event as a typed error. It is compiled only under C++23
(`#if __cplusplus >= 202302L` in `src/table_fsm.hpp` and `.cpp`), so the
mandatory C++20 build path never needs `std::expected`. The matching
gtest cases in `tests/table_fsm_test.cpp` are guarded the same way.
`apply()` keeps throwing `IllegalTransition` in both standards, since the
GoF machine, the `std::variant` machine, and the cross-check tests that
prove all three agree all rely on that shared exception contract.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/10-state-machines
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

Either path discovers the same 31 gtest cases under `cpp23` (29 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 31   # cpp23
100% tests passed, 0 tests failed out of 29   # cpp20
```

To see all four expressions walk the same lifecycle side by side, build
and run the demo binary. From this directory, `./demo.sh` does the same
three steps above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target state10_demo
./build/cpp23/state10_demo
```

Expected output:

```
== Expression 1: GoF State pattern ==
Start: Created
After pay(): Paid
After ship(): InTransit
After deliver(): Delivered

== Expression 2a: enum + transition table FSM ==
Same event sequence via the table: Delivered

== Expression 2b: std::variant + std::visit FSM ==
Same event sequence via the variant: Delivered

== C++23 refinement: typed rejection via std::expected ==
Rejected: cannot apply event 'Ship' to a shipment in status 'Created'

== Expression 3: stochastic FSM (seeded for reproducibility) ==
Seed 38, InTransit outcomes: InTransit InTransit InTransit Delivered
Final status: Delivered
```

The `std::expected` block only prints under the `cpp23` preset; under
`cpp20` the demo prints the variant block and then the stochastic block.
The stochastic block's sequence is identical under both presets, since
`std::mt19937` behaves the same way regardless of language standard.

## Files

```
examples/10-state-machines/
  CMakeLists.txt        # standalone + superbuild-integrated targets (state10_ prefix)
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py              # standalone Conan recipe (gtest/1.17.0)
  demo.sh                   # build + run the demo binary
  src/
    shipment_domain.hpp/.cpp    # ShipmentStatus, ShipmentEvent, IllegalTransition
    gof_state.hpp/.cpp          # expression 1: GoF State pattern
    table_fsm.hpp/.cpp          # expression 2a: enum + transition table FSM
    variant_fsm.hpp/.cpp        # expression 2b: std::variant + std::visit FSM
    stochastic_fsm.hpp/.cpp     # expression 3: weighted, RNG-injected FSM
    demo_main.cpp                 # exercises all four
  tests/
    gof_state_test.cpp
    table_fsm_test.cpp
    variant_fsm_test.cpp
    stochastic_fsm_test.cpp
    cross_check_test.cpp          # proves the three deterministic expressions agree
```
