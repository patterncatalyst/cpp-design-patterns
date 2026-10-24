# 06 — Value semantics, ownership, and smart pointers: a warehouse dispatch system

A parcel moving through a regional warehouse touches four different
ownership shapes: a value that gets copied freely, a resource one dock
owns outright, a contract more than one shipping route depends on, and a
manifest tree where a child needs to find its parent without keeping it
alive forever. This example builds all four, side by side, so you can
compare what each smart pointer actually buys you instead of treating them
as interchangeable ways to hold a pointer.

## The domain

- `Parcel` (`src/parcel.hpp`) — a plain value type (`std::string`,
  `double`), following the Rule of Zero: no custom destructor, copy, or
  move, because every member already has correct ones.
- `DockSlot` (`src/dock_slot.hpp/.cpp`) — holds at most one `Parcel` as a
  `std::unique_ptr<Parcel>`. `transfer_parcel()` moves a parcel from one
  slot to another; the source slot is empty afterward.
- `CarrierContract` (`src/carrier_contract.hpp/.cpp`) — a resource-owning
  type following the Rule of Five: it owns a raw, heap-allocated per-tier
  rate table (`Cents*`) with a hand-written destructor, copy constructor,
  copy assignment, move constructor, and move assignment.
- `ShippingRoute` (`src/shipping_route.hpp/.cpp`) — holds a
  `std::shared_ptr<CarrierContract>`. Two routes built from the same
  pointer share one contract: renegotiating a rate through either route is
  visible to both.
- `ManifestNode` (`src/manifest_tree.hpp/.cpp`) — a parent/child tree. A
  parent owns its children via `std::shared_ptr`; each child holds its
  parent via `std::weak_ptr`, breaking what would otherwise be a reference
  cycle. `make_root_manifest()` is the only way to create a root, since
  `shared_from_this()` requires the object already be owned by a
  `shared_ptr`.
- `legacy_rate_table.h/.cpp` + `rate_table_adapter.hpp/.cpp` — a
  deliberately C-style factory/free pair (`legacy_open_rate_table()` /
  `legacy_close_rate_table()`) and the adapter that wraps its raw pointer
  in a `std::unique_ptr<CRateTable, RateTableDeleter>`.

## C++23 refinement (guarded)

`open_rate_table()` in `src/rate_table_adapter.cpp` adopts the legacy
factory's raw pointer with `std::out_ptr(handle)` under C++23, and with a
named local `CRateTable*` passed by address under C++20
(`#if __cplusplus >= 202302L`). Both versions produce an identical
`RateTableHandle`; only the mechanism used to adopt the pointer differs.

## Build, run, observe

No containers — local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/06-value-semantics-ownership
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild — from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 22 gtest cases under both `cpp23` and
`cpp20` via `gtest_discover_tests` — `open_rate_table()` compiles to a
different implementation per standard but keeps the same signature, so no
test is compiled out between them. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 22   # cpp23
100% tests passed, 0 tests failed out of 22   # cpp20
```

To see all four ownership shapes side by side, build and run the demo
binary. From this directory, `./demo.sh` does the same three steps above
plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target value6_ownership_demo
./build/cpp23/value6_ownership_demo
```

Expected output:

```
== Value semantics: a Parcel copy is independent ==
original: TRK-1001, 4.2 kg, "Fragile - this side up"
copy:     TRK-1001, 9.9 kg, "Re-weighed at the hub"

== Sole ownership: unique_ptr transfers a Parcel between docks ==
before transfer: inbound occupied=yes, outbound occupied=no
after transfer:  inbound occupied=no, outbound occupied=yes (TRK-2002)

== Shared ownership: two routes quote against one CarrierContract ==
north-corridor quote (before renegotiation): $4.00
south-corridor quote (before renegotiation): $4.00
north-corridor quote (after renegotiation): $4.75
south-corridor quote (after renegotiation): $4.75

== Breaking a cycle: weak_ptr observes a manifest's parent ==
child's parent while root is alive: M-1000
child's parent after root is reset: expired

== C++23 refinement: adopting a legacy C factory's pointer ==
cardinal per-kg rate: 310 cents (handle releases it automatically)
unknown-carrier: rejected, no handle allocated
```

Running `examples/06-value-semantics-ownership/demo.sh cpp20` prints
identical output: `open_rate_table()`'s observable behavior does not
change between standards, only the mechanism it uses internally.

## Files

```
examples/06-value-semantics-ownership/
  CMakeLists.txt        # standalone + superbuild-integrated targets (value6_* names)
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py             # standalone Conan recipe (gtest/1.17.0)
  demo.sh                  # build + run the demo binary
  src/
    parcel.hpp                    # value type, Rule of Zero
    dock_slot.hpp/.cpp            # sole ownership: unique_ptr + move transfer
    carrier_contract.hpp/.cpp     # resource-owning type, Rule of Five
    shipping_route.hpp/.cpp       # shared ownership: shared_ptr
    manifest_tree.hpp/.cpp        # parent/child graph: shared_ptr + weak_ptr
    legacy_rate_table.h/.cpp      # C-style legacy factory/free pair
    rate_table_adapter.hpp/.cpp   # out_ptr (C++23) vs manual (C++20) adoption
    demo_main.cpp                  # exercises all four ownership shapes
  tests/
    parcel_value_semantics_test.cpp
    dock_slot_test.cpp
    carrier_contract_test.cpp
    manifest_tree_test.cpp
    rate_table_adapter_test.cpp
```
