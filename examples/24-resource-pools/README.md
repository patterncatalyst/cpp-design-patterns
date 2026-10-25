# 24 - Resource pools

A shipment-tracking service needs to reuse a handful of expensive-to-build
resources under a bound, and in several cases needs to decouple making a
call from whatever thread actually executes it. This example builds a
generic `ResourcePool<Resource>` with an RAII lease handle, a connection
pool as its canonical instance, and three sibling concurrency shapes from
the same domain: a thread pool, a monitor object, and an active object.

## The domain

- `TrackingStoreConnection` (`src/tracking_store_connection.hpp`): a
  connection to the shipment-tracking store, expensive to build, cheap to
  reuse.
- `ShippingLabelRequest` / `RenderedLabel` (`src/shipping_label.hpp`): the
  work a label-rendering task performs.
- `ShipmentManifestMonitor` (`src/shipment_manifest_monitor.hpp`): the
  line items inside one package, touched by concurrent warehouse scans.
- `PrintResult` (`src/label_printer_active_object.hpp`): the result of one
  label print request, including which thread actually executed it.

## Pattern one: the generic resource pool and its RAII lease

`ResourcePool<Resource>` (`src/resource_pool.hpp`) is the centerpiece: a
bounded, thread-safe pool built on a mutex and a condition variable, built
lazily up to a capacity, returned via a move-only `Lease` whose destructor
puts the resource back on the pool's free list on every exit path,
including exception unwinding. `acquire()` blocks with a timeout;
`try_acquire()` is the non-blocking sibling.

## Pattern two: the connection pool (canonical instance)

`TrackingStoreConnectionPool` (`src/tracking_store_connection_pool.hpp`)
wraps `ResourcePool<TrackingStoreConnection>` behind a `ConnectionFactory`
seam (`src/connection_factory.hpp`), so a test can substitute a
`MockConnectionFactory` and prove the pool calls `create()` at most
`capacity` times, no matter how many acquire/release cycles run against
it.

### C++23 refinement (guarded)

`try_acquire()` returns `std::expected<Lease, PoolAcquireError>` under
C++23, distinguishing `Exhausted` from `Closed`, instead of collapsing
both into one empty `std::optional<Lease>` under C++20. Guarded with
`#if __cplusplus >= 202302L` in `src/resource_pool.hpp`, so the mandatory
C++20 build path never needs `<expected>`.

## Pattern three: the thread pool

`LabelRenderThreadPool` (`src/label_render_thread_pool.hpp`) runs a fixed
set of `std::jthread` workers draining a shared task queue. `submit()`
returns a `std::future` immediately. The queue's internal `Task` type
differs by standard: C++20's `std::function` cannot hold a move-only
`std::packaged_task` directly and needs a `shared_ptr` wrapper, while
C++23's `std::move_only_function` stores it with no extra indirection.

## Pattern four: the monitor object

`ShipmentManifestMonitor` (`src/shipment_manifest_monitor.hpp`) owns its
own mutex; every public method locks it, so several warehouse-scan
threads can call `add_item()` concurrently on the same manifest without
corrupting it.

## Pattern five: the active object

`LabelPrinterActiveObject` (`src/label_printer_active_object.hpp`)
decouples invoking `print_label()` from executing it: the call only
enqueues a `std::packaged_task` and returns its `std::future`; a single
dedicated worker thread drains the request queue in submission order.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/24-resource-pools
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

Either path discovers the same 19 gtest cases under `cpp23` (18 under
`cpp20`: the one `std::expected`-specific case is compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 19   # cpp23
100% tests passed, 0 tests failed out of 18   # cpp20
```

To see all five patterns run end to end, build and run the demo binary.
From this directory, `./demo.sh` does the same steps above plus runs it:

```sh
./demo.sh cpp23
```

Expected output:

```
== Connection pool: bounded, blocking, reused ==
Leased connections 1 and 2; pool created=2 of capacity=2
try_acquire while exhausted: pool exhausted: every resource is currently leased
After releasing one lease, acquire() reused connection 1 (same id as before: yes); pool still created only 2 connection(s) total

== Thread pool: workers draining a shipping-label task queue ==
Rendered label for SHM-2001
Rendered label for SHM-2002
Rendered label for SHM-2003
Rendered label for SHM-2004
Rendered label for SHM-2005
All 5 submitted labels rendered by 3 worker thread(s)

== Monitor object: ShipmentManifestMonitor serializes concurrent scans ==
4 concurrent scanners each added 25 items; manifest now holds 100 items, total weight 100 kg

== Active object: LabelPrinterActiveObject decouples call from execution ==
Printed SHM-4001 on a different thread
Printed SHM-4002 on a different thread
Printed SHM-4003 on a different thread
```

Running `./demo.sh cpp20` prints the same output except the `try_acquire`
line, which reads "failed (no reason available)" instead of naming the
exhaustion reason, since `PoolAcquireError` only compiles under the
`cpp23` preset.

## Files

```
examples/24-resource-pools/
  CMakeLists.txt                       # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json                    # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                         # standalone Conan recipe (gtest/1.17.0)
  demo.sh                              # build + run the demo binary
  src/
    resource_pool.hpp                  # ResourcePool<Resource> + Lease (centerpiece)
    tracking_store_connection.hpp/.cpp # the pooled resource
    connection_factory.hpp/.cpp        # ConnectionFactory seam + live implementation
    tracking_store_connection_pool.hpp # the canonical pool instance
    shipping_label.hpp/.cpp            # ShippingLabelRequest/RenderedLabel + render_label()
    label_render_thread_pool.hpp/.cpp  # thread pool: workers draining a task queue
    shipment_manifest_monitor.hpp      # monitor object
    label_printer_active_object.hpp/.cpp  # active object
    demo_main.cpp                      # exercises all five patterns
  tests/
    resource_pool_test.cpp
    tracking_store_connection_pool_test.cpp
    label_render_thread_pool_test.cpp
    label_printer_active_object_test.cpp
    shipment_manifest_monitor_test.cpp
```
