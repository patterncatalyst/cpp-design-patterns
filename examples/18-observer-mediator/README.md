# 18 - Observer and Mediator: a shipment tracker and a dispatch board

Observer and Mediator both coordinate independent parts of a system, but
along different axes. `ShipmentTracker` fans a status change out to
whatever is currently subscribed, without ever knowing who, or how many,
listeners there are. `DispatchCoordinator` sits between three collaborators
that never reference each other directly, and is the only component that
knows who everyone is.

## Observer: `src/shipment_observer.hpp`

- `ShipmentStatus`: a shipment's lifecycle, `Created` through `Delivered`
  (or `Exception`).
- `StatusChangeSlot`: the real subscription primitive, a
  `std::function<void(tracking_id, old_status, new_status)>`. A caller
  with nothing but a lambda can subscribe without writing a class.
- `ShipmentObserver`: the classic Gang of Four interface, for a subscriber
  that is a named, reusable collaborator with its own identity.
  `subscribe(ShipmentObserver&)` is sugar over `subscribe(StatusChangeSlot)`.
- `DashboardView` and `AlertingService`: concrete `ShipmentObserver`
  implementations.
- `ScopedSubscription`: the RAII handle every `subscribe()` call returns.
  Letting it go out of scope unsubscribes automatically, which is what
  keeps a destroyed observer from ever being notified again, provided the
  subscription is declared (and so destroyed) before the observer it
  belongs to.

### Avoiding a dangling observer

`tests/shipment_observer_test.cpp`'s `ScopedDashboardObserver` shows the
pattern this example relies on: declare the observer member first and the
`ScopedSubscription` member second, so C++'s reverse-declaration-order
destruction tears the subscription down before the observer itself is
destroyed. By the time anything could notify into the observer, the
subscription that would have triggered that call no longer exists.

`advance_status()` also notifies from a snapshot copy of the subscriber
list rather than the live one, so a slot that unsubscribes (itself or
another slot) from inside its own callback cannot invalidate the list
`advance_status()` is still iterating.

## Mediator: `src/dispatch_mediator.hpp`

- `DispatchEvent`: the four events a colleague can raise
  (`OrderReadyForPickup`, `PickedUpByCarrier`, `OutForDelivery`,
  `DeliveryException`).
- `DispatchColleague`: the shared base. It holds a mediator pointer and
  nothing else, no reference to any sibling colleague.
- `WarehouseStation`, `CarrierStation`, `CustomerNotificationStation`:
  concrete colleagues, each exposing only the operations its own role
  needs.
- `DispatchCoordinator`: the mediator. Its constructor binds all three
  colleagues to itself (breaking the circular construction order a direct
  reference cycle would otherwise require) and `notify()` routes each
  event to the collaborator that should respond.

Calling `warehouse.mark_order_ready("ORD-5001")` cascades through the
coordinator to `CarrierStation::confirm_pickup()`, which itself raises a
second event the coordinator routes to
`CustomerNotificationStation::send()`, all without any of the three
colleagues holding a reference to another.

### C++23 refinement (guarded)

`ShipmentTracker::try_subscribe()` returns
`std::expected<ScopedSubscription, std::string>` instead of throwing,
rejecting a subscribe call against a shipment that already reached a
terminal status (`Delivered` or `Exception`) with a typed error. It is
compiled only under C++23 (`#if __cplusplus >= 202302L` in
`src/shipment_observer.hpp` and `.cpp`), so the mandatory C++20 build path
never needs `std::expected`. The matching gtest cases in
`tests/shipment_observer_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/18-observer-mediator
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

Either path discovers the same 14 gtest cases under `cpp23` (12 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the fan-out and the dispatch cascade produce real output rather
than just reading an assertion that says they work, run the example's own
demo script. From this directory, `./demo.sh` does the same three steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target observe18_demo
./build/cpp23/observe18_demo
```

Expected output:

```
== Observer: interface-based subscribers fan out on a status change ==
Dashboard now reads: TRK-7001: InTransit
Dashboard now reads: TRK-7001: Exception
Alerts raised: 1
  TRK-7001 flagged: InTransit -> Exception

== Observer: a std::function callback needs no observer class at all ==
TRK-7002: Created -> PickedUp
TRK-7002: PickedUp -> InTransit
TRK-7002: InTransit -> Delivered

== Observer: unsubscribing stops notification, even mid-session ==
Callback ran 1 time(s); subscriber_count() is now 0

== Mediator: a warehouse event cascades through the coordinator ==
Carrier's own record of pickups: 1
Messages the coordinator routed to the customer:
  ORD-5001: Your package has been picked up and is on its way.
  ORD-5001: Your package is out for delivery today.
  ORD-5001: There is a delivery exception on your package; our team is investigating.

== C++23 refinement: typed rejection of a subscribe after a terminal status ==
Rejected subscribe: cannot subscribe to a shipment that already reached a terminal status
```

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the mediator section.

## Files

```
examples/18-observer-mediator/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    shipment_observer.hpp/.cpp   # ShipmentTracker, ScopedSubscription, DashboardView, AlertingService
    dispatch_mediator.hpp/.cpp   # DispatchCoordinator + the three colleague stations
    demo_main.cpp                 # exercises both halves
  tests/
    shipment_observer_test.cpp
    dispatch_mediator_test.cpp
```
