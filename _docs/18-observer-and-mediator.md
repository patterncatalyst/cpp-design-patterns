---
title: "Observer and Mediator"
order: 18
part: "coordinating-traversing"
description: "Two coordination patterns for a many-party system, taught against a shipment tracker and a warehouse dispatch board: Observer fans a status change out to independently subscribed listeners, from a classic interface down to a lightweight std::function callback, and Mediator centralizes a many-to-many conversation behind one hub so a warehouse, a carrier, and a notification service never reference each other directly."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force each pattern answers: Observer frees a subject from
  knowing who is listening to it; Mediator frees a set of peers from
  knowing about each other at all.
- Build an Observer subject that fans a status change out to independently
  subscribed listeners, both through a classic interface and through a
  `std::function` callback, and say when the callback form is enough on
  its own.
- Manage a subscription with an RAII handle so a destroyed observer never
  gets notified again, and explain why that ordering matters.
- Build a Mediator that routes events between a warehouse, a carrier, and
  a customer-notification component, none of which ever references
  another directly.
- Tell the two patterns apart by the shape of the communication each one
  centralizes: Observer's one-to-many fan-out versus Mediator's
  many-to-many routing through a single hub.
- Choose between throwing when a subscribe call reaches a shipment already
  in a terminal status and C++23's `std::expected` for the same rejection,
  and say which standard each belongs to.
- Recognize when either pattern is coordinating collaborators that have no
  real reason to avoid knowing about each other, and say what to write
  instead.

## Why this matters

A shipment's status changes many times between the moment a warehouse
packs it and the moment it lands on a doorstep, and more than one part of
a shipping system cares every time it does. A support dashboard wants the
latest status to show up the instant it changes. An alerting service wants
to know the moment a shipment flips into an exception state, and nothing
before that. A billing dispute, months later, wants a durable record of
every transition a specific tracking number went through. None of those
three concerns is the status change itself; each one is something that
happens *in response to* it, added independently of the others and on its
own schedule. Observer is the Gang of Four's name for exactly this force:
a subject broadcasts one fact, that its state changed, to an open set of
listeners it never has to enumerate by name, add import statements for, or
even know exist.

A warehouse dispatch board has a related problem with a different shape. A
warehouse stages an order and hands it to a carrier; the carrier picks it
up, moves it through delivery, and occasionally reports a problem; a
customer-notification service has to hear about every one of those events
and turn it into a message a customer reads. If the warehouse called the
carrier's method directly, and the carrier called the notification
service's method directly, adding a fourth collaborator, a
returns-processing stage that needs to hear about delivery exceptions too,
would mean editing every existing class that stage needs to hear from. The
number of possible direct wires between components grows far faster than
the number of components. Mediator is the Gang of Four's name for
breaking that mesh: no warehouse, carrier, or notification component ever
calls another one's method, and every one of them talks only to a single
coordinator standing between them. The coordinator is the one place in the
system that is allowed to know who everyone is and what each of them is
for.

Both patterns loosen a connection that would otherwise bind two parts of a
system directly to each other, but they loosen it along different axes.
Observer frees a subject from needing to know who is listening to it, and
nothing about an observer's identity ever reaches the subject's own code.
Mediator frees a set of peers from knowing about each other at all, and
pays for that with a hub that does know about every one of them. That
distinction, not how similar their code looks, is what separates the two
patterns.

{% include excalidraw.html name="18-observer-mediator" caption="Figure 18.1: A ShipmentTracker subject fanning a status change out to independent observers, next to a DispatchCoordinator mediator routing between three collaborators that never reference each other directly, shown against the tangled direct mesh it replaces." %}

## The domain: a shipment tracker and a dispatch board

The example behind this chapter lives in `examples/18-observer-mediator`;
the run script there builds and runs it, and its `README.md` covers what
it does and how to drive it. Both halves of the example share one small
enum, a shipment's lifecycle from creation to delivery:

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

The Observer half is `ShipmentTracker`, the subject that owns a shipment's
current status and notifies whoever has subscribed every time that status
moves. The Mediator half is `DispatchCoordinator`, which sits between
three independent components, `WarehouseStation`, `CarrierStation`, and
`CustomerNotificationStation`, and decides which one responds to which
event. Nothing in either half of the example reuses a type from the other;
they are two separate forces, taught against the same shipping operation
because both show up in the same system at once, not because one calls
into the other.

## Observer: a subject that does not know who is listening

`ShipmentTracker` holds one piece of state, a `ShipmentStatus`, and a list
of subscribed callbacks. What a subscriber is, underneath, is a plain
function signature:

```cpp
using StatusChangeSlot =
    std::function<void(const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status)>;
```

`subscribe(StatusChangeSlot)` stores one of these and returns a handle;
`advance_status()` moves the tracked status forward and calls every
currently-subscribed slot with the tracking ID, the old status, and the
new one, in the order each slot subscribed. A subject built this way has
no class hierarchy to maintain and no base interface a subscriber must
implement to participate; anything with the right call signature,
including a lambda with captured state, is a valid subscriber.

Alongside that lightweight slot, this example also keeps the classic Gang
of Four shape, an interface a subject notifies through a reference:

```cpp
class ShipmentObserver {
public:
    virtual ~ShipmentObserver() = default;
    virtual void on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                                    ShipmentStatus new_status) = 0;
};
```

`DashboardView` and `AlertingService` both implement `ShipmentObserver`.
Subscribing one of them calls the same `subscribe(StatusChangeSlot)` path
underneath, through a second overload that wraps the observer's
`on_status_changed` in a slot:

```cpp
ScopedSubscription ShipmentTracker::subscribe(ShipmentObserver& observer) {
    return subscribe([&observer](const std::string& tracking_id, ShipmentStatus old_status,
                                  ShipmentStatus new_status) {
        observer.on_status_changed(tracking_id, old_status, new_status);
    });
}
```

That ordering is deliberate: the interface-based form is sugar over the
callback-based one, not a parallel mechanism the subject has to support
twice. `ShipmentTracker` only ever stores
`StatusChangeSlot` values; `subscribe(ShipmentObserver&)` exists purely to
give a named, reusable collaborator, one with its own identity and
potentially other methods besides `on_status_changed`, a way to plug into
the same list.

## When the language makes Observer cheap

A caller with nothing but a lambda does not need `ShipmentObserver` at
all. This example's audit trail subscribes directly, with no class
anywhere:

```cpp
std::vector<std::string> audit_entries;
auto audit_sub = tracker.subscribe(
    [&audit_entries](const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status) {
        audit_entries.push_back(tracking_id + ": " + to_string(old_status) + " -> " + to_string(new_status));
    });
```

That is the callback the task of logging every transition needs:
one closure, capturing the vector it appends to, with no virtual
dispatch, no separate translation unit, and nothing to name in a header. A
language without first-class functions forces every observer through an
interface no matter how small its job is, which is why the textbook
Observer pattern is drawn as a class diagram in the first place. C++ does
not have that constraint, so reach for `ShipmentObserver` only when an
observer earns a name of its own: `DashboardView` is a long-lived object a
different part of the program holds onto and queries (`current_status_line()`),
and `AlertingService` accumulates a list of alerts another component reads
later. Neither is a one-shot reaction; both are collaborators with state
and an identity, which is exactly the case the interface form is for. A
callback that only ever runs once per notification and keeps no identity
of its own, like the audit log, does not need that ceremony.

## The `ScopedSubscription` handle: closing the dangling-observer gap

The classic hazard in every Observer implementation is a destroyed
observer still sitting in a subject's list, waiting for a notification
that will call into memory that no longer holds a live object. This
example closes that hole with an RAII handle rather than a manual
`unsubscribe()` call a caller has to remember to make. `subscribe()`
returns a `ScopedSubscription`, and that object's destructor removes the
subscription from `ShipmentTracker`'s list automatically:

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

The guarantee this buys depends on getting one ordering right: the
subscription has to be destroyed *before* the observer it belongs to. C++
destroys an object's members in the reverse of the order they were
declared, which gives a clean way to enforce that ordering with nothing
more than field placement:

```cpp
class ScopedDashboardObserver {
public:
    explicit ScopedDashboardObserver(ShipmentTracker& tracker) : subscription_(tracker.subscribe(dashboard_)) {}

    DashboardView dashboard_;

private:
    ScopedSubscription subscription_;
};
```

`dashboard_` is declared first and `subscription_` second, so when a
`ScopedDashboardObserver` goes out of scope, `subscription_` is torn down
first, unsubscribing from the tracker, and only afterward is `dashboard_`
itself destroyed. By the time anything could call into a dangling
`DashboardView`, there is no subscription left to call through. This is
the same discipline Chapter 5 built around deterministic lifetimes,
applied to this specific hazard: Observer's safety does not come from
discipline at every call site, it comes from one class whose constructor
and destructor enforce an ordering a caller cannot get wrong by forgetting
a step.

`advance_status()` guards against one more lifetime hazard. A slot is free
to unsubscribe itself, or trigger an unsubscribe of a different slot, from
inside its own callback, so `advance_status()` notifies from a snapshot
copy of the subscriber list rather than iterating the live one:

```cpp
const auto snapshot = slots_;
for (const auto& [id, slot] : snapshot) {
    slot(tracking_id_, old_status, new_status);
}
```

Without that copy, a slot unsubscribing mid-notification would invalidate
the iterator the loop is using to walk the list it just changed. Copying a
handful of `std::function` values once per status change is a small, fixed
cost against a failure that is hard to reproduce, showing up only when a
subscriber's own callback modifies the subscription list it is being
called from.

## Mediator: centralizing a many-to-many conversation

`DispatchColleague` is the shared base every component in the Mediator
half inherits from. It holds no pointer to `WarehouseStation`, no pointer
to `CarrierStation`, and no pointer to `CustomerNotificationStation`: the
only thing a colleague can reach is its own mediator:

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

`WarehouseStation::mark_order_ready()` calls `raise(DispatchEvent::OrderReadyForPickup, order_id)`,
and that call has no idea which collaborator, if any, responds. Wiring a
colleague to a mediator happens once, in `DispatchCoordinator`'s
constructor, through the private `bind()` the coordinator reaches as a
friend of the base class:

```cpp
DispatchCoordinator::DispatchCoordinator(WarehouseStation& warehouse, CarrierStation& carrier,
                                          CustomerNotificationStation& notifier)
    : warehouse_(&warehouse), carrier_(&carrier), notifier_(&notifier) {
    warehouse.bind(*this);
    carrier.bind(*this);
    notifier.bind(*this);
}
```

That constructor order matters for a reason specific to Mediator: every
colleague needs a mediator reference, but the coordinator needs live
colleague objects to construct itself from, which is a circular
requirement if both sides try to hold the other by reference from the
start. Binding after construction, through a method only the coordinator
is allowed to call, breaks the cycle: a `WarehouseStation` can exist with
no mediator at all until something wires it to one, and nothing about its
own interface exposes that it can be rewired.

Routing itself is a single method on the coordinator, a switch over the
event with no branch that lets a colleague skip the hub:

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

Calling `warehouse.mark_order_ready("ORD-5001")` in this example cascades
through two hops the warehouse itself never has to know about:
`WarehouseStation` raises `OrderReadyForPickup`, the coordinator calls
`CarrierStation::confirm_pickup()`, and that call itself raises
`PickedUpByCarrier`, which the same coordinator routes to
`CustomerNotificationStation::send()`. Three components took part in one
customer-visible outcome, and at no point did any one of them hold a
reference to another. That cascade also runs synchronously: `notify()`
calls straight through to the next colleague's method before returning.
Chapter 11 covers the shape a dispatch takes when it needs to be deferred
or batched rather than run immediately: `CommandQueue`'s FIFO drain is the
pattern to reach for once a coordinator's work has to wait for a scheduled
job rather than fire the moment an event is raised.

> **AI sidebar.** Ask an LLM pairing partner to wire together a handful of
> components that need to react to each other's events, and a common
> result has every component holding a `shared_ptr` or raw pointer to
> every other component it needs to call, because that is the shape that
> requires the least new vocabulary to produce. It compiles, and for three
> components it even reads clearly. Check what happens to that generated
> code the moment a fourth collaborator needs to hear about the same
> event: every existing class that needs to know about it gets a new
> member and a new call, and the number of wires between components grows
> faster than the number of components does. A `DispatchCoordinator`-style
> hub costs one extra hop per event and buys a system where adding a
> collaborator means teaching the hub about it once, not editing every
> peer that happens to care. A generated class holding references to three
> sibling classes it calls directly, with no coordinator anywhere in
> sight, is the clearest sign the model reached for the shape with the
> fewest moving parts today instead of the one that stays flat as the
> collaborator count grows.

## Typed rejection: subscribing after a terminal status

Subscribing to a `ShipmentTracker` that has already reached `Delivered` or
`Exception` cannot ever produce a notification; there is no future status
change left to report. A C++20 build rejects that call by throwing:

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

C++23's `std::expected` reports the identical rejection as a typed value
instead, checkable with an `if` and no exception machinery at all.
`try_subscribe()` is guarded behind `#if __cplusplus >= 202302L` precisely
so the mandatory C++20 build path never needs `std::expected`, while the
C++23 build gets the more precise contract.

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

Both versions enforce the identical terminal-status rule. Only how a
caller learns about a rejected subscribe changes between the two columns,
the same narrow scope this book's codetabs convention has held to since
Chapter 8's typed Strategy error and Chapter 16's typed Proxy access
check.

## When not to use Observer or Mediator

Both patterns earn their cost when there is a real reason a subject must
not know who depends on it, or a real reason a set of peers must not know
about each other. Outside those two specific cases, each one adds
indirection with nothing underneath it to justify the cost.

A status change with exactly one piece of code that ever reacts to it does
not need `ShipmentTracker`'s subscription machinery at all; it needs a
direct function call at the point the status changes. Wrapping a single,
fixed reaction in a subscribe-and-notify pipeline before a second listener
is in hand or clearly coming adds a `std::function` allocation, a
subscription to manage, and a reader's detour through a class definition
to find the one call site that will ever use it. The plainer non-pattern
is calling the handler directly, the same candidate-non-pattern move
Chapter 1 trained for every pattern in this book.

Mediator fails a related test in a narrower way. Three components that
only ever grow by one and never gain a fourth collaborator do not need a
hub standing between them; a plain, visible sequence of calls, read start
to finish by whoever maintains that code, is the whole story and a
reviewer can confirm it is correct without jumping to a coordinator class
to see who talks to whom. The point where that calculus flips is the
point this chapter's warehouse board crossed: more than a handful of
collaborators, different teams owning different ones, or a routing rule
that changes independently of any single collaborator's own logic.
Introducing `DispatchCoordinator` ahead of any of those three pressures
defends against a wiring problem the system does not have yet, at the
cost of a layer of indirection every reader now has to trace through to
follow one event from cause to effect.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/18-observer-mediator` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset,
12 under `cpp20`, the difference being the two `std::expected`-specific
cases that only compile under C++23. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the fan-out and the dispatch cascade produce real output rather
than just reading an assertion that says they work, run the example's own
demo script:

```sh
examples/18-observer-mediator/demo.sh cpp23
```

It subscribes a dashboard and an alerting service to a tracked shipment,
advances its status through an exception, subscribes a plain callback as
an audit log on a second shipment, shows a subscription going out of scope
mid-session, then runs an order through the dispatch coordinator end to
end:

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

The alert line shows the exact transition that triggered it,
`InTransit -> Exception`, confirming `AlertingService` only reacts to the
one transition it cares about and not to the `PickedUp` or `InTransit`
changes that preceded it. The mediator block shows three messages
reaching the customer from a single `mark_order_ready()` call plus two
direct `CarrierStation` calls, none of which required the warehouse, the
carrier, or the notification service to hold a reference to any of the
others. Running `examples/18-observer-mediator/demo.sh cpp20` prints the
same output minus the final `std::expected` block, which only compiles
under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design
  Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley):
  the original Observer and Mediator entries, including the "one-to-many
  dependency" and "encapsulate how a set of objects interact" intent
  statements this chapter's tracker and dispatch board each build on.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official
  GoogleTest documentation: the testing idiom behind
  `examples/18-observer-mediator`'s suite, which asserts fan-out order,
  post-unsubscribe silence, and correct event routing rather than only
  checking that each class constructs without error.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
