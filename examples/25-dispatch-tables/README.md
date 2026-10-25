# 25 - Dispatch tables and maps

A shipment-tracking ingestion service receives carrier webhook messages
that have to be routed to the right handler by message type, with no
`if`/`else if` chain anywhere in the routing path. This example builds the
same routing job three idiomatic ways and contrasts their costs: an
enum-indexed function-pointer table (a jump table), a dispatch map for an
open, runtime-extensible set of message kinds, and a `constexpr
std::array` table resolved and checked complete at compile time.

## The domain

- `CarrierMessageType` / `CarrierMessage` (`src/carrier_message.hpp`): the
  closed set of message kinds this codebase compiles against: `Created`,
  `PickedUp`, `InTransit`, `OutForDelivery`, `Delivered`,
  `DeliveryException`.
- `CarrierWireMessage` (`src/carrier_message.hpp`): the open set, a raw
  string `wire_tag` plus a tracking id and a detail string, standing in for
  whatever a third-party carrier's webhook actually sends on the wire.
- `TrackingSink` / `TrackingLedger` (`src/tracking_ledger.hpp`): the
  receiver every dispatch form writes into, recording a note per tracking
  id.
- `message_handlers.hpp`/`.cpp`: the handler functions every dispatch form
  routes to, one per `CarrierMessageType`, plus a fallback for an
  unrecognized type.

## Form one: enum-indexed jump table

`src/jump_table.hpp`/`.cpp`. `g_jump_table` is a plain C array of
`MessageHandler` function pointers, one slot per `CarrierMessageType`, in
the enum's declaration order. `dispatch_via_jump_table()` converts the
message's type to an index and calls through the matching slot, falling
back to the unrecognized-type handler when the index is out of range. The
table's correctness depends entirely on the slots staying in the same
order as the enum; nothing in the type system enforces that.

## Form two: dispatch map (open, runtime-extensible)

`src/dispatch_map.hpp`/`.cpp`. `MessageDispatchMap` wraps a
`std::unordered_map<std::string, Handler>` keyed by wire tag, seeded in its
constructor with the wire tags this codebase already knows about.
`register_handler()` lets a new carrier's wire tag be added at runtime,
after construction, with no change to this class and no recompile.
`dispatch()` falls back to an unrecognized-tag handler when a wire tag
resolves to nothing.

Under C++23, `FlatMessageDispatchMap` (guarded by
`#if __cplusplus >= 202302L`) offers the same open contract backed by
`std::flat_map` instead: two contiguous, sorted vectors rather than a
node-based hash table, a better fit for a table that is written to rarely
(one registration per onboarded carrier) and read from on every inbound
message.

## Form three: constexpr std::array table

`src/dispatch_table_constexpr.hpp`/`.cpp`. `make_dispatch_table()` builds
the same closed set as the jump table, but assigns each handler to an
explicit index derived from the enumerator itself inside a `constexpr`
function, and a `static_assert` right below it proves every slot got
filled before the translation unit finishes compiling. Adding a
`CarrierMessageType` and forgetting its line here fails the build at that
`static_assert`, instead of shipping a null function pointer the jump table
would call without complaint.

### C++23 refinements (guarded)

- `message_type_index()` (`src/carrier_message.hpp`) uses `std::to_underlying`
  under C++23 to name the enum-to-index conversion explicitly, falling back
  to a bare `static_cast` under C++20.
- `FlatMessageDispatchMap` (`src/dispatch_map.hpp`/`.cpp`) is compiled only
  under C++23 (`#if __cplusplus >= 202302L`), so the mandatory C++20 build
  path never needs `<flat_map>`. The matching gtest cases in
  `tests/dispatch_map_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/25-dispatch-tables
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

Either path discovers this example's gtest cases via
`gtest_discover_tests`: 14 cases under `cpp23` (12 under `cpp20`: the two
`std::flat_map`-specific cases are compiled out).

To see all three forms, the closed-set unknown fallback, the open map's
late registration, and the C++23 `std::flat_map` variant run end to end,
build and run the demo binary. From this directory, `./demo.sh` does the
same steps above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target dispatch25_demo
./build/cpp23/dispatch25_demo
```

## Files

```
examples/25-dispatch-tables/
  CMakeLists.txt              # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json           # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                # standalone Conan recipe (gtest/1.17.0)
  demo.sh                     # build + run the demo binary
  src/
    carrier_message.hpp              # CarrierMessageType, CarrierMessage, CarrierWireMessage, message_type_index()
    tracking_ledger.hpp/.cpp         # TrackingSink interface + TrackingLedger receiver
    message_handlers.hpp/.cpp        # the handler functions every form routes to
    jump_table.hpp/.cpp              # Form one: enum-indexed function-pointer table
    dispatch_table_constexpr.hpp/.cpp  # Form three: constexpr std::array table + completeness static_assert
    dispatch_map.hpp/.cpp            # Form two: open dispatch map (+ C++23 std::flat_map variant)
    demo_main.cpp                    # exercises all three forms
  tests/
    mock_tracking_sink.hpp           # gmock collaborator double
    recording_tracking_sink.hpp      # plain recording fake
    jump_table_test.cpp
    constexpr_table_test.cpp
    dispatch_map_test.cpp
```
