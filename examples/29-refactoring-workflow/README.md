# 29 - Fowler and Feathers: contrasted and combined

One inherited shipping component, a delivery ETA estimator, taken through
the full combined workflow: Feathers first (characterize, then break a
dependency), Fowler next (named refactorings on the now-tested code).
Three stages, three estimator types, one behavior preserved end to end.

## The domain

- `LegacyEtaEstimator` (`src/legacy_eta_estimator.hpp`): STAGE 1. The
  inherited code as it arrived. A six-argument `estimate()` call, an
  if/else carrier dispatch, and a direct
  `std::chrono::system_clock::now()` read buried in the method body with
  no way for a caller to substitute a different "today."
- `transit_calculations.hpp` (`base_transit_days()`, `base_surcharge()`):
  the part of the inherited logic that was already testable before
  anything changed, pure functions with no hidden collaborator.
- `Clock` / `SystemClock` / `FixedClock` (`src/clock.hpp`): the seam
  Extract Interface creates around "today," plus the real implementation
  and the test double the seam exists to make possible.
- `SeamedEtaEstimator` (`src/seamed_eta_estimator.hpp`): STAGE 2. Extract
  Interface and Parameterize Constructor applied to `LegacyEtaEstimator`.
  The carrier dispatch and surcharge math are byte-for-byte unchanged;
  only the clock read moved behind the seam.
- `ShipmentProfile` (`src/shipment_profile.hpp`): Introduce Parameter
  Object's result, replacing the six positional arguments with one named
  value.
- `CarrierProfile` / `GroundCarrierProfile` / `AirCarrierProfile` /
  `ExpressCarrierProfile` / `FreightCarrierProfile`
  (`src/carrier_profile.hpp`): Replace Conditional with Polymorphism's
  result, replacing the carrier if/else chain with one small class per
  carrier.
- `ModernEtaEstimator` (`src/modern_eta_estimator.hpp`): STAGE 3. Extract
  Function, Replace Conditional with Polymorphism, and Introduce
  Parameter Object applied to `SeamedEtaEstimator`. The clock seam from
  stage 2 is kept exactly as it was.

## Why the stages run in this order

`LegacyEtaEstimator::estimate()` reads the wall clock on its own. A
characterization test can pin the deterministic parts of its behavior
(`base_transit_days()`, `base_surcharge()`) immediately, because those are
already pure functions. It cannot pin an exact `delivery_date` or an exact
holiday-adjusted `transit_days`, because both change depending on what
calendar month the test suite happens to run in.
`tests/legacy_characterization_test.cpp` proves exactly this gap: it pins
everything that can be pinned and states, as an invariant rather than an
exact value, what the rest of `estimate()` does.

`SeamedEtaEstimator` closes that gap without touching anything else.
`tests/dependency_break_test.cpp` injects a `FixedClock` at a known date
and pins the exact `delivery_date`, `transit_days`, and `surcharge` that
were previously impossible to predict, then re-asserts every
characterization value from stage 1 to prove the carrier dispatch and
surcharge rules never moved.

Only once both of those are green does `ModernEtaEstimator` touch the
shape of the code. `tests/modern_refactor_test.cpp` runs the identical
inputs through `SeamedEtaEstimator` and `ModernEtaEstimator` side by side
and asserts they agree, for every carrier and every surcharge combination
already under test, then adds a fourth carrier (`FreightCarrierProfile`)
to show the refactored shape extends without editing the three profiles
that already existed.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/29-refactoring-workflow
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
`gtest_discover_tests`.

To see all three stages run side by side rather than read as an
assertion, build and run the demo binary. From this directory, `./demo.sh`
does the same steps above plus runs it:

```sh
examples/29-refactoring-workflow/demo.sh cpp23
```

The demo prices the same ground shipment through the legacy estimator
(whatever today happens to be), the seamed estimator (pinned to two fixed
dates, one inside and one outside the holiday window), and the modern
estimator (the identical two fixed dates, plus a freight shipment on a
carrier that did not exist before the refactor).

## Files

```
examples/29-refactoring-workflow/
  CMakeLists.txt             # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json          # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py               # standalone Conan recipe (gtest/1.17.0)
  demo.sh                    # build + run the three-stage demo binary
  src/
    eta_estimate.hpp                  # the shared result value all three stages return
    clock.hpp                         # Clock seam: Clock, SystemClock, FixedClock
    transit_calculations.hpp/.cpp     # the already-testable part of the inherited logic
    legacy_eta_estimator.hpp/.cpp     # STAGE 1: inherited code, wall clock read inline
    seamed_eta_estimator.hpp/.cpp     # STAGE 2: Extract Interface + Parameterize Constructor
    shipment_profile.hpp              # Introduce Parameter Object's result
    carrier_profile.hpp/.cpp          # Replace Conditional with Polymorphism's result
    modern_eta_estimator.hpp/.cpp     # STAGE 3: Fowler's named refactorings applied
    demo_main.cpp                     # runs all three stages side by side
  tests/
    legacy_characterization_test.cpp  # stage 1: pins what's pinnable, names what isn't
    dependency_break_test.cpp         # stage 2: pins the previously-impossible cases
    modern_refactor_test.cpp          # stage 3: equivalence + extensibility proof
```
