# 14 - Singleton, and the dependency-injection alternative

A shipment fulfillment pipeline needs an audit trail: every warehouse
packing event, billing charge, and carrier handoff recorded somewhere every
other module can find it. This example builds that capability two ways,
side by side: a thread-safe Meyers singleton, and the same capability as an
abstraction injected by constructor, wired once by a composition root.

## The domain

- `format_fulfillment_event()` (`src/audit_domain.hpp/.cpp`) builds the one
  message text both versions record, so a wording fix lands in both at
  once and the two versions differ only in how the dependency is reached.
- Both versions record the identical fact: an order was fulfilled.

## The two versions

1. **Singleton** (`src/shipment_audit_log_singleton.hpp/.cpp`):
   `ShipmentAuditLog::instance()` returns a thread-safe, function-local
   `static` reference, reached from anywhere with no constructor parameter
   announcing the dependency. `reset_for_testing()` exists only so this
   example's own tests can isolate themselves from the log's shared global
   state, a test-only escape hatch whose presence is itself evidence of the
   design's cost.
2. **Dependency injection** (`src/audit_sink.hpp/.cpp` +
   `src/order_fulfillment_service.hpp/.cpp`): `AuditSink` is the
   abstraction, `InMemoryAuditSink` the one production implementation, and
   `OrderFulfillmentService` is the high-level policy that depends on
   `AuditSink&`, injected through its constructor, following the same
   manual-DI shape as `examples/04-solid-dependency-inversion`.
   `demo_main.cpp` is the composition root that wires a concrete sink to
   the service that needs it.

### C++23 refinement (guarded)

`OrderFulfillmentService::try_fulfill_order()` returns
`std::expected<FulfillmentReceipt, std::string>` instead of throwing or
returning a sentinel, reporting a failed audit-sink write with a typed
reason. It is compiled only under C++23
(`#if __cplusplus >= 202302L` in `src/order_fulfillment_service.hpp` and
`.cpp`), so the mandatory C++20 build path uses `std::optional` instead.
The matching gtest cases in `tests/order_fulfillment_service_test.cpp` are
guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/14-singleton-and-di
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

Either path discovers the same 9 gtest cases under both `cpp23` and
`cpp20` via `gtest_discover_tests` (the two `try_fulfill_order`-specific
cases compile against `std::expected` under one standard and
`std::optional` under the other, rather than being compiled out
entirely). Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 9   # cpp23
100% tests passed, 0 tests failed out of 9   # cpp20
```

To see the singleton and the injected version side by side, build and run
the demo binary. From this directory, `./demo.sh` does the same three steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target single14_audit_demo
./build/cpp23/single14_audit_demo
```

Expected output:

```
== Singleton: ShipmentAuditLog::instance() ==
[warehouse] packed ORD-2001
[billing] charged ORD-2001
Same instance from both call sites: yes

== Dependency injection: OrderFulfillmentService ==
Order ORD-2002 fulfilled
sink entry: Order ORD-2002 fulfilled

== C++23 refinement: typed failure via std::expected ==
Rejected: audit sink failed to record order ORD-2003
```

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the dependency-injection section.

## Files

```
examples/14-singleton-and-di/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    audit_domain.hpp/.cpp                 # shared fulfillment-event text
    shipment_audit_log_singleton.hpp/.cpp # Meyers singleton
    audit_sink.hpp/.cpp                   # AuditSink + InMemoryAuditSink
    order_fulfillment_service.hpp/.cpp    # DI high-level policy
    demo_main.cpp                          # exercises both versions
  tests/
    shipment_audit_log_singleton_test.cpp
    order_fulfillment_service_test.cpp
    fakes.hpp
```
