# 05 - RAII and deterministic lifetimes: a warehouse dock bay lease

A distribution center has a fixed number of loading-dock bays, and only
one labeling station can use a bay at a time. This example ties the
lifetime of "this bay is in use" to the lifetime of a C++ object instead
of a bool flag a caller has to remember to flip back, and tests that the
tie holds under every way a scope can end: a normal return, an early
return, and an exception.

## The domain

- `DockBayRegistry`: a fixed number of dock bays, each either free or
  leased. `try_lease()`/`release()` are plain, non-throwing bookkeeping;
  the registry itself has no notion of RAII.
- `DockBayLease` (`src/dock_bay_lease.hpp`): the RAII guard. Acquires a
  bay in its constructor (or throws `BayUnavailableError`), releases it in
  its destructor unconditionally. Copy is deleted; move transfers
  ownership and leaves the source with nothing to release.
- `ScopeGuard` (`src/scope_guard.hpp`): a general-purpose, domain-free
  lambda-on-destruction utility, with a `dismiss()` escape hatch for a
  commit path that no longer needs its rollback to run.
- `label_shipment_batch()` (`src/labeling_station.hpp`): the piece that
  uses both together: it leases a bay for a whole batch of shipments and
  uses a `ScopeGuard` to roll a label counter back to zero if a bad
  barcode throws partway through.

## What the tests prove

- **Release at scope exit.** A `DockBayLease` created inside a block
  releases its bay the moment the block ends, no explicit call anywhere
  (`dock_bay_lease_test.cpp`).
- **Release on exception.** A lease still releases its bay when the
  function that holds it throws, proven both directly
  (`ReleasesTheBayWhenAnExceptionUnwindsThePossessingScope`) and through
  the labeling station's batch, which also proves a second guard
  (`ScopeGuard`) rolls back a counter on the same unwind
  (`labeling_station_test.cpp`).
- **No double release.** `DockBayRegistry::release_count()` counts only
  leased-to-free transitions. After a move (`MoveConstructionTransfers...`)
  or a move-assignment onto an already-owning lease
  (`MoveAssignmentReleasesTheTargetsPriorBayBeforeAdoptingTheNewOne`), two
  destructors run on the same `bay_id`, but `release_count()` advances by
  exactly one, proving the moved-from object's destructor is a true no-op
  rather than a second release that happens not to matter yet.
- **LIFO ordering.** `ScopeGuardTest.RunsActionsInLifoOrderLikeStackDestructors`
  declares three guards in one scope and records the order their actions
  run in; it comes back reversed, the same order C++ runs stack-allocated
  destructors in.

## C++23 refinement (guarded)

`DockBayLease::try_acquire()` returns `std::expected<DockBayLease, std::string>`
instead of throwing, compiled only under C++23
(`#if __cplusplus >= 202302L` in `src/dock_bay_lease.hpp` and `.cpp`), so
the mandatory C++20 build path never needs `std::expected`. The matching
gtest cases in `tests/dock_bay_lease_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/05-raii-lifetimes
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

Either path discovers the same 15 gtest cases under `cpp23` (13 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 15   # cpp23
100% tests passed, 0 tests failed out of 13   # cpp20
```

To see the lease, the exception path, and the `ScopeGuard` rollback run
side by side, build and run the demo binary. From this directory,
`./demo.sh` does the same steps above plus runs it; or build just the
demo target after a `cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target raii5_demo
./build/cpp23/raii5_demo
```

## Files

```
examples/05-raii-lifetimes/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    dock_bay_registry.hpp/.cpp   # plain bookkeeping, no RAII
    dock_bay_lease.hpp/.cpp      # the RAII guard over one dock bay
    scope_guard.hpp               # general lambda-on-destruction utility
    labeling_station.hpp/.cpp     # lease + ScopeGuard used together
    demo_main.cpp                  # exercises all of the above
  tests/
    dock_bay_lease_test.cpp
    scope_guard_test.cpp
    labeling_station_test.cpp
```
