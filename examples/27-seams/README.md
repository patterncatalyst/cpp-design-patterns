# 27 - Seams and the test safety net

An inherited shipment-SLA checker reads the wall clock directly, which
makes its exact behavior at the deadline boundary untestable. This
example characterizes what the inherited code actually does, introduces
an object seam so the clock becomes substitutable, and then changes the
boundary rule with the test suite staying green throughout. A second,
smaller routine shows the link seam: a bare free function with no object
to inject a collaborator into.

## The domain

- `LegacyShipmentSlaChecker` (`src/legacy_shipment_sla_checker.hpp`):
  BEFORE. Calls `std::chrono::system_clock::now()` directly inside
  `is_breached()`. No constructor argument, no seam, no way for a test to
  control what moment it believes "now" is.
- `Clock` / `SystemClock` (`src/clock.hpp`): the object seam itself. An
  interface standing in for "the current moment," with `SystemClock` the
  one production implementation.
- `ShipmentSlaChecker` (`src/shipment_sla_checker.hpp`): AFTER. Takes a
  `const Clock&` through its constructor instead of reading the system
  clock. Also carries a deliberate rule change from the legacy checker: a
  deadline exactly equal to "now" counts as breached, a boundary the
  legacy checker could never be tested against at all.
- `grace_policy.hpp` / `grace_policy_production.cpp` /
  `tests/grace_policy_test_double.cpp`: the link seam. `grace_period()` is
  declared once and defined twice; which definition ends up in the final
  binary depends entirely on which `.cpp` a given CMake target compiles.
- `late_fee_schedule.hpp` (`late_fee_cents()`): a bare free function with
  no object to inject anything into, which is why its `grace_period()`
  dependency is broken with a link seam instead of an object seam.

## What the characterization tests prove, and what they can't

`tests/legacy_shipment_sla_checker_characterization_test.cpp` pins
`LegacyShipmentSlaChecker`'s real, observed behavior at a two-hour margin
in each direction: well-overdue shipments are breached, well-ahead
shipments are not. Both tests need that wide margin because the checker
reads the real wall clock; a test cannot force it to land on an exact
instant it controls. That gap, not any particular test, is the direct
cost of a hard dependency: some behavior cannot even be characterized
until it is reachable through a seam.

## What the object seam proves

`tests/shipment_sla_checker_test.cpp` proves two things about
`ShipmentSlaChecker`, which refactors the legacy checker to take a
`Clock` through its constructor:

1. **The seam alone changed nothing** that the legacy characterization
   tests already covered: with a real `SystemClock`, the same two-hour
   margins produce the same answers.
2. **The seam makes the exact boundary testable**, and a `MockClock`
   (GoogleMock) is used to pin it precisely: a deadline exactly equal to
   "now," one nanosecond before it, and one nanosecond after it, each a
   deterministic, instant test with no sleeping and no margin.

`ShipmentSlaChecker::is_breached()` also encodes a deliberate change from
the legacy rule: the exact-deadline case now counts as breached, where
the legacy checker treated it as not yet breached. That change was made
safely because the seam existed first; the corrected boundary is pinned
by a test, not left to a reviewer's eyeball.

## What the link seam proves

`tests/late_fee_schedule_test.cpp` asserts `late_fee_cents(5)` equals
`750`. Under production's fifteen-minute grace period
(`src/grace_policy_production.cpp`), five minutes late would still fall
inside the grace window and accrue nothing; `750` is only correct because
`seam27_tests` compiles `tests/grace_policy_test_double.cpp` instead,
which returns a zero-minute grace period. `late_fee_schedule.cpp` is
compiled exactly once, into `seam27_core`, and never recompiles or
changes between the two binaries; only the linker's choice of object
file does.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/27-seams
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

Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 11   # cpp23
100% tests passed, 0 tests failed out of 11   # cpp20
```

The cpp20 build has one fewer `std::expected`-flavored test than cpp23
and one `std::optional`-flavored test in its place; `gtest_discover_tests`
reports eleven cases under both presets either way, because
`try_time_remaining()`'s two standards each contribute exactly one test
covering the overdue case and one covering the not-yet-overdue case.

To see the legacy checker, the seamed checker, and the link-seamed late
fee schedule side by side at the terminal, run the example's own demo
script:

```sh
examples/27-seams/demo.sh cpp23
```

Expected output:

```
== Legacy checker: hard dependency on the wall clock, no seam ==
legacy SHM-7001 breached: true
legacy SHM-7002 breached: false

== Seamed checker: Clock injected through the constructor ==
seamed SHM-7001 breached: true
seamed SHM-7002 breached: false

== Late fee schedule: grace period reached through the link seam ==
5 minutes late: 0 cents (within production's 15-minute grace)
25 minutes late: 1500 cents
```

Running `examples/27-seams/demo.sh cpp20` prints identical output; the
demo binary never calls `try_time_remaining()`, so nothing about it is
standard-dependent.

## Files

```
examples/27-seams/
  CMakeLists.txt                              # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json                           # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                                # standalone Conan recipe (gtest/1.17.0)
  demo.sh                                     # build + run the demo binary
  src/
    shipment_sla.hpp                          # ShipmentSla value type
    clock.hpp/.cpp                            # Clock interface + SystemClock (the object seam)
    legacy_shipment_sla_checker.hpp/.cpp      # BEFORE: hard dependency on the wall clock
    shipment_sla_checker.hpp/.cpp             # AFTER: Clock injected; deliberate boundary-rule change
    grace_policy.hpp                          # grace_period() declaration only
    grace_policy_production.cpp               # grace_period() production definition (linked into seam27_demo)
    late_fee_schedule.hpp/.cpp                # late_fee_cents(): the link seam's motivating free function
    demo_main.cpp                             # exercises all three pieces side by side
  tests/
    mock_clock.hpp                                        # GoogleMock Clock double
    legacy_shipment_sla_checker_characterization_test.cpp # pins the legacy checker's real behavior
    shipment_sla_checker_test.cpp                         # proves the seam, the exact boundary, and try_time_remaining()
    late_fee_schedule_test.cpp                            # proves the link seam took effect
    grace_policy_test_double.cpp                          # grace_period() test definition (linked into seam27_tests)
```
