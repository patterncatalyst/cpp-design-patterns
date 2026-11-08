---
title: "Observer and Mediator"
order: 18
part: "coordinating-traversing"
description: "Two coordination patterns for a many-party system, taught against a shipment tracker and a warehouse dispatch board: Observer fans a status change out to independently subscribed listeners, from a classic interface down to a lightweight std::function callback, and Mediator centralizes a many-to-many conversation behind one hub so a warehouse, a carrier, and a notification service never reference each other directly."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force each pattern answers: Observer frees a subject from knowing who listens; Mediator frees peers from knowing about each other.
- Build an Observer subject that fans a status change out through a classic interface and through a `std::function` callback, and say when the callback is enough.
- Manage a subscription with an RAII handle so a destroyed observer is never notified.
- Build a Mediator that routes events between a warehouse, a carrier and a customer-notification component that never reference each other.
- Tell the two apart by shape: one-to-many fan-out versus many-to-many routing through a hub.
- Choose between throwing on a subscribe after a terminal status and returning `std::expected`, new in C++23.
- Recognize when collaborators have no reason to avoid knowing each other.

## Why this matters

A shipment's status changes many times between packing and the doorstep, and several parts of the system care. A support dashboard wants the latest status. An alerting service wants to know when a shipment enters an exception state, and nothing else. A billing dispute months later wants every transition a tracking number went through. Each reacts to the change, and each is added on its own schedule.

A warehouse dispatch board has a different shape. A warehouse stages an order for a carrier; the carrier picks it up, delivers it and sometimes reports a problem; a notification service turns each event into a customer message. If each component calls the next directly, adding a returns stage that also hears about delivery exceptions means editing every class it hears from. Direct wires grow faster than components.

Both patterns loosen a direct connection, along different axes. Observer keeps a subject ignorant of its listeners. Mediator keeps peers ignorant of each other and pays with a hub that knows them all.

{% include excalidraw.html name="18-observer-mediator" caption="Figure 18.1: A ShipmentTracker subject fanning a status change out to independent observers, next to a DispatchCoordinator mediator routing between three collaborators that never reference each other directly, shown against the tangled direct mesh it replaces." %}

Both halves share a shipment's lifecycle:

```cpp
enum class ShipmentStatus {
    Created,
    PickedUp,
    InTransit,
    OutForDelivery,
    Delivered,
    Exception,
};
```

## Pattern: Observer

### The problem

`ShipmentTracker` owns a shipment's status. The dashboard, the alerting service and the audit log all need to hear when it moves, and the tracker shouldn't name, include or know about any of them.

### The solution

**An Observer lets a subject notify an open set of subscribers when its state changes, without knowing who they are.**

### How C++ models it

A subscriber is a function signature:

```cpp
using StatusChangeSlot =
    std::function<void(const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status)>;
```

`subscribe(StatusChangeSlot)` stores a slot and returns a handle. `advance_status()` calls every subscribed slot with the tracking ID, the old status and the new one, in subscription order. An unchanged status notifies no one. Anything with the right call signature, including a lambda with captured state, is a subscriber.

The classic interface sits on top of that:

```cpp
class ShipmentObserver {
public:
    virtual ~ShipmentObserver() = default;
    virtual void on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                                    ShipmentStatus new_status) = 0;
};
```

`DashboardView` and `AlertingService` implement it, and subscribing one wraps it in a slot:

```cpp
ScopedSubscription ShipmentTracker::subscribe(ShipmentObserver& observer) {
    return subscribe([&observer](const std::string& tracking_id, ShipmentStatus old_status,
                                  ShipmentStatus new_status) {
        observer.on_status_changed(tracking_id, old_status, new_status);
    });
}
```

The tracker stores only `StatusChangeSlot` values. The interface form is sugar over the callback, not a second mechanism.

### Callback or interface

The audit trail needs no class:

```cpp
std::vector<std::string> audit_entries;
auto audit_sub = tracker.subscribe(
    [&audit_entries](const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status) {
        audit_entries.push_back(tracking_id + ": " + to_string(old_status) + " -> " + to_string(new_status));
    });
```

The textbook Observer is a class diagram because its languages lacked first-class functions. In C++, choose by what the subscriber is:

| Subscriber | Form | Example |
| --- | --- | --- |
| A reaction with no identity of its own | Lambda slot | The audit log |
| A long-lived object other code queries | `ShipmentObserver` | `DashboardView::current_status_line()` |
| An object that accumulates state for later readers | `ShipmentObserver` | `AlertingService`'s alert list |

### The `ScopedSubscription` handle

The classic Observer hazard is a destroyed observer still in the subject's list. `subscribe()` returns a `ScopedSubscription` whose destructor removes the entry, so no caller has to remember `unsubscribe()`:

```cpp
ScopedSubscription::~ScopedSubscription() {
    release();
}

void ScopedSubscription::release() {
    if (tracker_ != nullptr) {
        tracker_->unsubscribe(id_);
        tracker_ = nullptr;
    }
}
```

The subscription must die before the observer it points at. Members are destroyed in reverse declaration order, so field placement enforces it:

```cpp
class ScopedDashboardObserver {
public:
    explicit ScopedDashboardObserver(ShipmentTracker& tracker) : subscription_(tracker.subscribe(dashboard_)) {}

    DashboardView dashboard_;

private:
    ScopedSubscription subscription_;
};
```

`subscription_` is declared after `dashboard_`, so it's destroyed first and unsubscribes before the `DashboardView` goes away. The ordering lives in one class, not in every caller.

### Notify from a snapshot

A slot may unsubscribe itself, or another slot, from inside its callback. Iterating the live list would invalidate the loop's iterator, so `advance_status()` notifies from a copy:

```cpp
const auto snapshot = slots_;
for (const auto& [id, slot] : snapshot) {
    slot(tracking_id_, old_status, new_status);
}
```

Copying a handful of `std::function` values per status change is a small fixed cost against a failure that appears only when a callback edits the list it's called from.

### A typed rejection

A tracker that has reached `Delivered` or `Exception` has no future change to report, so subscribing to it is an error. `std::expected` reports that as a value instead of an exception. `try_subscribe()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never sees it.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on a subscribe-after-terminal-status call works, but it
// forces every caller to wrap subscribe() in a try block or accept an
// unhandled exception on a status-tracking call.
ScopedSubscription ShipmentTracker::subscribe(StatusChangeSlot slot) {
    if (is_terminal(status_)) {
        throw std::logic_error("cannot subscribe to a shipment that already reached a terminal status");
    }
    const std::size_t id = next_id_++;
    slots_.emplace_back(id, std::move(slot));
    return ScopedSubscription(*this, id);
}
```
```cpp
// C++23: std::expected reports the same rejection as a typed value,
// checkable with an if and no exception machinery.
std::expected<ScopedSubscription, std::string> ShipmentTracker::try_subscribe(StatusChangeSlot slot) {
    if (is_terminal(status_)) {
        return std::unexpected<std::string>("cannot subscribe to a shipment that already reached a terminal status");
    }
    const std::size_t id = next_id_++;
    slots_.emplace_back(id, std::move(slot));
    return ScopedSubscription(*this, id);
}
```

Both enforce the same terminal-status rule. Only how a caller learns about a rejection changes.

## Pattern: Mediator

### The problem

`WarehouseStation`, `CarrierStation` and `CustomerNotificationStation` each react to the others' events. Wiring them directly gives every class a pointer to every class it calls, and a fourth collaborator means editing each of them.

### The solution

**A Mediator encapsulates how a set of objects interact, so each object talks only to the mediator and never to its peers.**

### How C++ models it

Every colleague derives from `DispatchColleague`, which holds no pointer to any other station. The only thing a colleague can reach is its mediator:

```cpp
class DispatchColleague {
public:
    virtual ~DispatchColleague() = default;

protected:
    void raise(DispatchEvent event, const std::string& order_id);

private:
    friend class DispatchCoordinator;
    void bind(DispatchMediator& mediator) { mediator_ = &mediator; }

    DispatchMediator* mediator_ = nullptr;
};
```

`WarehouseStation::mark_order_ready()` calls `raise(DispatchEvent::OrderReadyForPickup, order_id)` without knowing who responds. The coordinator wires each colleague once, in its constructor, through the private `bind()` it reaches as a friend:

```cpp
DispatchCoordinator::DispatchCoordinator(WarehouseStation& warehouse, CarrierStation& carrier,
                                          CustomerNotificationStation& notifier)
    : warehouse_(&warehouse), carrier_(&carrier), notifier_(&notifier) {
    warehouse.bind(*this);
    carrier.bind(*this);
    notifier.bind(*this);
}
```

Colleagues need a mediator, and the coordinator needs live colleagues to construct from. Binding after construction, through a method only the coordinator may call, breaks that cycle. A station can exist unbound, and its public interface doesn't expose rewiring.

### Routing in one place

Routing is one switch, with no path that lets a colleague skip the hub:

```cpp
void DispatchCoordinator::notify(DispatchColleague& sender, DispatchEvent event, const std::string& order_id) {
    switch (event) {
        case DispatchEvent::OrderReadyForPickup:
            carrier_->confirm_pickup(order_id);
            break;
        case DispatchEvent::PickedUpByCarrier:
            notifier_->send(order_id, "Your package has been picked up and is on its way.");
            break;
        // ...
    }
}
```

`warehouse.mark_order_ready("ORD-5001")` cascades through two hops the warehouse never sees. The warehouse raises `OrderReadyForPickup`, the coordinator calls `CarrierStation::confirm_pickup()`, that raises `PickedUpByCarrier`, and the coordinator routes it to `CustomerNotificationStation::send()`. The cascade is synchronous: `notify()` calls the next colleague before returning. When dispatch must wait for a scheduled job, queue Command objects and drain them instead.

### Observer or Mediator

| | Observer | Mediator |
| --- | --- | --- |
| Shape | One subject, many listeners | Many peers, one hub |
| Who knows whom | Subject knows no listener | Hub knows every peer; peers know only the hub |
| Adding a participant | Subscribe it | Teach the hub its events |
| Cost | A `std::function` and a subscription per listener | One extra hop per event, one class that grows with the rules |

## When not to use it

- **Observer.** A status change with one fixed reaction needs a direct call, not a subscription pipeline. Add the pipeline when a second listener is in hand or clearly coming; until then it costs a `std::function` allocation, a handle to manage and a detour for every reader.
- **Mediator.** Three components with a stable set of calls read best as a visible sequence a reviewer follows top to bottom. Add a hub when collaborators multiply, different teams own them, or the routing rule changes independently of any one collaborator.

<blockquote class="ai-sidebar" markdown="1">
**Specifying event wiring.** "Wire these components to react to each other" leaves open whether components hold pointers to each other or talk through a hub, and who removes a subscription. State both:

> Route every event between `WarehouseStation`, `CarrierStation` and `CustomerNotificationStation` through `DispatchCoordinator::notify()`; no station holds a pointer or reference to another. Colleagues call `raise()` from `DispatchColleague`, and only the coordinator calls `bind()`. For status changes, have `ShipmentTracker::subscribe()` return a `ScopedSubscription` whose destructor unsubscribes, and notify from a copy of `slots_`.

Run `ctest --preset cpp23 -R 'DispatchCoordinator|ShipmentTracker'`, and check that no station class has a member pointing at another station.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/18-observer-mediator`; its `README.md` covers driving the demo:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 cases pass under `cpp23` and 12 under `cpp20`; the two `std::expected` cases compile only under C++23. To see the fan-out and the cascade:

```sh
examples/18-observer-mediator/demo.sh cpp23
```

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

`AlertingService` raises one alert, for `InTransit -> Exception` only. The three customer messages come from one `mark_order_ready()` call plus two direct `CarrierStation` calls, with no station holding a reference to another. Under `cpp20` the output stops before the `std::expected` block.

- **Forget to unsubscribe.** In a scratch copy, empty the body of `ScopedSubscription::~ScopedSubscription()` in `src/shipment_observer.cpp`. Two of the 14 cases fail, and the observer-owns-its-subscription case crashes with a segmentation fault when the tracker notifies a destroyed `DashboardView`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Observer and Mediator entries, including the "one-to-many dependency" and "encapsulate how a set of objects interact" intent statements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/18-observer-mediator`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
