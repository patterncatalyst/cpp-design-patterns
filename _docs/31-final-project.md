---
title: "Final project: composing patterns into a system"
order: 31
part: "putting-it-together"
description: "The book's final project: a shipment dispatch console that composes Strategy, Factory, State, Command, and Observer into one coherent system, taught against the seams where the patterns meet rather than re-teaching any one of them from scratch."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Compose Strategy, Factory, State, Command, and Observer into one running
  system instead of five unrelated demonstrations, and name the object
  that owns each pattern's role.
- Identify the seams where two patterns meet: where a Factory hands back a
  Strategy, where a Command invokes a Strategy, where a Command drives a
  State machine, and where a State transition fires an Observer event.
- Design a single composition root that wires every pattern together
  without any collaborator needing to know about the others.
- Write an integration test that proves the composed system works end to
  end, a full lifecycle run with a computed rate, fired notifications, and
  a reversed correction, rather than five isolated unit tests that never
  touch each other.
- Recognize when composing this many patterns into one system is
  over-engineering for a smaller problem, and say what a smaller system
  should do instead.

{% include excalidraw.html name="31-final-project" caption="Figure 31.1: ShipmentDispatcher as the composition root, with Factory handing back a Strategy-bearing CarrierProfile, Command driving both State and Strategy at their seams, and State notifying Observer on every transition." %}

## Why this matters

Every pattern in this book up to this point has been taught the same way:
one force, one worked example, one focused test suite. That's the right
way to learn what Strategy buys you, or what Command's undo costs, in
isolation. It's also not how any of these patterns show up in a real
system. A real shipment dispatch console does not have a "Strategy
chapter" and a separate "State chapter" running side by side and never
talking to each other. It has one `Shipment` whose lifecycle fires events
that something else reacts to, one rate calculation that something else
invokes on demand, and one carrier-selection step that hands the right
pieces to everything downstream of it. The patterns meet at seams, and a
team that has only ever seen each pattern taught alone is unprepared for
the moment two of them have to cooperate inside the same method.

This final project builds that system. It composes five patterns, Strategy
for rate selection, Factory for carrier creation, State for the shipment
lifecycle, Command for reversible operator actions, and Observer for
lifecycle notifications, into one `ShipmentDispatcher` that a warehouse
operator's console could plausibly run. The worked example in this chapter
does not re-explain what Strategy is or why State rejects an illegal
transition; it assumes a reader already knows that, by name, and spends
its words instead on the four places these patterns touch: a Factory
handing back a Strategy, a Command invoking a Strategy, a Command driving
a State machine, and a State transition firing an Observer notification.
Those four seams, not the five patterns individually, are what this
chapter is about.

## The domain: a shipment dispatch console

The system lives in `examples/31-final-project`. A `Shipment` carries an
id, a carrier name, a destination zone, a weight in kilograms, and a
quoted rate in cents. It also carries a status: `Created`, `PickedUp`,
`InTransit`, or `Delivered`. Three events, `PickUp`, `Dispatch`, and
`Deliver`, move it forward one step at a time. `Shipment::advance()`
enforces that order directly:

{% raw %}
```cpp
void Shipment::advance(ShipmentEvent event) {
    ShipmentStatus next;
    switch (status_) {
        case ShipmentStatus::Created:
            if (event != ShipmentEvent::PickUp) throw IllegalTransitionError(status_, event);
            next = ShipmentStatus::PickedUp;
            break;
        case ShipmentStatus::PickedUp:
            if (event != ShipmentEvent::Dispatch) throw IllegalTransitionError(status_, event);
            next = ShipmentStatus::InTransit;
            break;
        case ShipmentStatus::InTransit:
            if (event != ShipmentEvent::Deliver) throw IllegalTransitionError(status_, event);
            next = ShipmentStatus::Delivered;
            break;
        default:
            throw IllegalTransitionError(status_, event);
    }
    const ShipmentStatus previous = status_;
    status_ = next;
    notify(previous, status_);
}
```
{% endraw %}

That's the whole State component: a context that owns its current status,
rejects an event that does not apply, and only calls `notify()` once a
transition succeeds. `Shipment` is also the Observer component's
subject. It holds a list of `LifecycleObserver*` subscribers and fans a
transition out to every one of them the moment `advance()` accepts it.
Putting both roles on one type is not an accident of a small example. It
is the first seam this chapter cares about: the State machine's success
condition and the Observer's trigger condition are the same event, with
nothing in between them to get out of sync.

## Seam one: Factory hands back a Strategy

`CarrierFactory::create()` takes a `CarrierRequest`, a carrier code and a
destination zone, and returns a `CarrierProfile`: a carrier name, a
service-level commitment in hours, and a `std::unique_ptr<RateStrategy>`
already pointed at the correct concrete implementation.

```cpp
if (request.carrier_code == "SKYHAUL") {
    return CarrierProfile{
        .carrier_name = "Skyhaul Express",
        .sla_hours = 24,
        .rate_strategy = std::make_unique<WeightTieredRateStrategy>(std::vector<WeightTier>{
            WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
            WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
            WeightTier{.max_weight_kg = 50.0, .base_cents = 2400, .cents_per_kg = 95.0},
        }),
    };
}
```

Nothing calling `CarrierFactory` ever names `WeightTieredRateStrategy` or
`FlatRateStrategy` directly. `ShipmentDispatcher::open_shipment()` asks for
a carrier by code, gets back a `CarrierProfile`, and immediately asks that
profile's `rate_strategy` for the shipment's opening quote:

```cpp
carrier_profile_ = factory_.create(request);
shipment_.emplace(std::move(shipment_id), carrier_profile_->carrier_name,
                   request.destination_zone, weight_kg, Cents{0});
shipment_->set_quoted_rate_cents(carrier_profile_->rate_strategy->quote(*shipment_));
```

This is the seam where Factory and Strategy meet: Factory's whole
job is choosing which Strategy a caller gets, and the caller never has to
know the choice was made. A new carrier with a third pricing rule needs
one new `RateStrategy` subclass and one new branch inside
`CarrierFactory::create()`. `ShipmentDispatcher` does not change at all,
because it was never written against a concrete strategy in the first
place.

## Seam two: Command drives State

`AdvanceLifecycleCommand` wraps exactly one call to `Shipment::advance()`.
Its `execute()` records the status before the call so its `undo()` has
something to restore:

```cpp
void AdvanceLifecycleCommand::execute() {
    previous_status_ = shipment_.status();
    shipment_.advance(event_);
    executed_ = true;
}

void AdvanceLifecycleCommand::undo() {
    shipment_.restore_status_for_undo(previous_status_);
}
```

It's tempting to assume Command's undo for a State transition should call
some reverse event, an "un-pick-up," to walk the machine backward. Real
carriers do not support that operation, and neither does this one.
`restore_status_for_undo()` is a separate, narrower entry point that skips
the transition table and sets the status back to whatever it was. That's
the same technique this book's Command material uses for any action with
no clean arithmetic inverse: capture a snapshot before the change, restore
the snapshot instead of computing an inverse operation. The snapshot still
notifies every subscribed Observer, because an operator correcting a
mis-scanned pickup is itself a real event a dashboard should see, not a
rewind no one hears about.

`ShipmentDispatcher::make_advance_command()` is where this seam becomes
visible at the call site:

```cpp
std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_advance_command(ShipmentEvent event) {
    return std::make_unique<AdvanceLifecycleCommand>(shipment(), event);
}
```

An operator console only reaches `shipment.advance()` through a command
the dispatcher builds; the dispatcher's `CommandHistory` is what runs it.
That indirection is what makes every lifecycle change undoable by
construction, rather than undoable only for the operations someone
remembered to wrap.

## Seam three: Command invokes Strategy

A warehouse rescale corrects a shipment's recorded weight after it was
already quoted. `AdjustWeightCommand` is where Command and Strategy meet
directly: its `execute()` does not just set a new number, it asks the same
`RateStrategy` the carrier was built with for a fresh quote at the
corrected weight.

```cpp
void AdjustWeightCommand::execute() {
    previous_weight_kg_ = shipment_.weight_kg();
    previous_quoted_rate_cents_ = shipment_.quoted_rate_cents();

    shipment_.set_weight_kg(new_weight_kg_);
    shipment_.set_quoted_rate_cents(rate_strategy_.quote(shipment_));
    executed_ = true;
}
```

`undo()` does not call the strategy a second time. It restores the exact
`previous_weight_kg_` and `previous_quoted_rate_cents_` captured before the
correction ran, which is both cheaper and more correct than recomputing:
recomputing assumes the strategy is a pure function of the shipment's
current fields, and while that happens to be true here, a snapshot does
not depend on that assumption holding for every future `RateStrategy`
someone adds.

`ShipmentDispatcher::make_adjust_weight_command()` closes over the exact
same strategy object the factory built for this shipment's carrier:

```cpp
std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_adjust_weight_command(double new_weight_kg) {
    return std::make_unique<AdjustWeightCommand>(shipment(), *carrier_profile().rate_strategy, new_weight_kg);
}
```

That single line is the whole point of composing Factory and Strategy
before composing Command on top of them: a weight correction and a
shipment's opening quote are always priced by the identical rule, because
they're, literally, the same `RateStrategy` instance.

## Seam four: State fires Observer

The fourth seam already appeared in `Shipment::advance()` above: a legal
transition calls `notify()`, and `notify()` walks every subscribed
`LifecycleObserver` and calls `on_transitioned()` on each one.

```cpp
void Shipment::notify(ShipmentStatus from, ShipmentStatus to) {
    for (LifecycleObserver* observer : observers_) {
        observer->on_transitioned(*this, from, to);
    }
}
```

The demo program's `ConsolePrintingObserver` prints a line to the
terminal. A real deployment might wire the same interface to a customer
notification service, a tracking dashboard, and a billing ledger, three
subscribers that never reference each other and never reference the
`AdvanceLifecycleCommand` that triggered the event three calls up the
stack. The observer does not know, and should not need to know, that a
Command was involved at all. It only knows a `Shipment` changed status.
That separation is what lets the Command and State seam described above
evolve independently of how many subscribers are listening, or what any
one of them does with the notification.

## The composition root

`ShipmentDispatcher` is the one type in this example that knows about all
five patterns at once. Every other component in the system only ever sees
the interface directly above or below it: `CarrierFactory` does not know
about `Shipment`, `AdjustWeightCommand` does not know about
`CarrierFactory`, and `LifecycleObserver` implementations do not know
`ShipmentCommand` exists. `ShipmentDispatcher` is where `open_shipment()`
calls the factory, `make_advance_command()` and
`make_adjust_weight_command()` wire a command to the shipment and, where
relevant, to the factory-built strategy, and `apply()` and `undo_last()`
run every command through a `CommandHistory`.

## Factory's rejection path: C++20 versus C++23

`CarrierFactory` is the one component in this system with a real
C++20/23 difference worth showing, because its rejection path is exactly
the kind of caller-facing contract this book's idiomatic-expressions
material cares about: how a lookup failure is reported should not change
which concrete `RateStrategy` a caller ends up with.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: CarrierFactory::create() throws for a carrier code it does not
// recognize; a caller that wants an explicit rejection reason catches the
// exception directly.
CarrierProfile CarrierFactory::create(const CarrierRequest& request) const {
    if (request.carrier_code == "GROUNDLINE") { /* ... */ }
    if (request.carrier_code == "SKYHAUL") { /* ... */ }
    throw std::invalid_argument("CarrierFactory: unrecognized carrier_code '" + request.carrier_code + "'");
}
```
```cpp
// C++23: try_create() is implemented once, on top of create(), and returns
// a typed std::expected a caller can branch on without touching an
// exception at the call site. Neither concrete RateStrategy, nor
// ShipmentDispatcher, needed a single line changed to gain this.
std::expected<CarrierProfile, CarrierFactoryError> CarrierFactory::try_create(const CarrierRequest& request) const {
    try {
        return create(request);
    } catch (const std::invalid_argument&) {
        return std::unexpected(CarrierFactoryError::UnknownCarrierCode);
    }
}
```

> **AI sidebar.** Ask an LLM pairing partner to "add retry logic around the
> carrier lookup" or "cache the rate strategy for performance" on a system
> like this one, and it will often happily hand back a patch that touches
> `ShipmentDispatcher`, `CarrierFactory`, and `AdjustWeightCommand` all at
> once, because from the model's point of view they're just three nearby
> files in the same directory. What it will not do on its own is notice
> that `AdjustWeightCommand` holds a `const RateStrategy&` precisely so a
> cached or swapped strategy stays valid for the lifetime of a command that
> might still be sitting on the undo stack, or that `CarrierFactory`
> returning a `CarrierProfile` by value, rather than a shared pointer, is
> what keeps one shipment's pricing rule from leaking into another's.
> Before accepting a generated change to a composed system like this one,
> trace which seam the change crosses. A change that looks local
> to one file can break an invariant two patterns away from it, and the
> four seams named above are the places to check first.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain from the prerequisites; there's no container anywhere in this
loop. From the repository root, the root superbuild picks up
`examples/31-final-project` automatically:

{% include excalidraw.html name="31-final-project-sequence" caption="Figure 31.2: The time-ordered sequence of one shipment's run, with the same RateStrategy instance quoting the opening rate at step 2 and the corrected rate at step 5." %}

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`CarrierFactory::try_create()` only exists under C++23, so the two presets
discover a different number of gtest cases through `gtest_discover_tests`.
Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 34   # cpp23
100% tests passed, 0 tests failed out of 32   # cpp20
```

Five of the six test files each check one pattern's own contract: the
lifecycle's legal and illegal transitions, the two rate strategies'
arithmetic, the factory's carrier lookup, and each command's execute and
undo. The sixth,
`tests/shipment_dispatcher_integration_test.cpp`, is this example's actual
point. It never repeats a single-pattern check; it drives the whole
dispatcher through a lifecycle run, confirms a factory-built strategy
priced the opening quote, confirms two lifecycle commands fire Observer
notifications in order, confirms a weight-correction command recomputes
through that same strategy, and confirms undoing that command restores
both the weight and the quote exactly, all inside one running system.

To see the composition play out rather than read about it, run the
example's own demo script:

```sh
examples/31-final-project/demo.sh cpp23
```

```
== Opening a shipment with Skyhaul Express ==
  carrier: Skyhaul Express (SLA 24h)
  opening quote at 8kg: $27.20

== Advancing the lifecycle: PickUp, Dispatch ==
  [observer] SHIP-4821: Created -> PickedUp
  [observer] SHIP-4821: PickedUp -> InTransit
  status now: InTransit

== A warehouse rescale corrects the weight to 15kg ==
  weight now: 15kg, recomputed quote: $37.00

== The rescale turns out to be a scanner glitch; undo it ==
  weight restored to: 8kg, quote restored to: $27.20

== Delivering the shipment ==
  [observer] SHIP-4821: InTransit -> Delivered
  status now: Delivered

== Operator mis-scanned PickUp again by habit; the console rejects it ==
  rejected: illegal transition: event 'PickUp' does not apply to status 'Delivered'
```

Every number in that output traces back to one of the four seams above:
the opening quote comes from Factory handing back Strategy, the two
`[observer]` lines come from State firing Observer, the corrected and
restored quotes come from Command invoking and then snapshotting around
Strategy, and the final rejection comes from State's own transition table
refusing an event that no longer applies. Running
`examples/31-final-project/demo.sh cpp20` prints identical output; nothing
in the demo path depends on a C++23-only feature.

## When not to use it

Composing five patterns into one system is a cost, not a default, and the
same forces that justify any one of these patterns on its own are what
decide whether composing several of them is worth paying for here.

A shipment that never changes status, never needs an undo, and prices
against a single carrier does not need a `ShipmentDispatcher` at all. A
function that takes a weight and a carrier code and returns a cents value
solves that problem completely, and wrapping it in Strategy, Factory, and
a composition root to match is solving a problem the system does not have
yet. This is the same speculative-generality trap this book's anti-pattern
material warns against, just distributed across five classes instead of
one.

A system with exactly one carrier does not need Factory at all; a single
concrete `RateStrategy` constructed directly is simpler and says
everything the Factory indirection would have said, with one less seam to
reason about. Reach for Factory when a second carrier, and a second
concrete strategy behind it, is a real, current requirement, not a
plausible future one.

A system where no operation ever needs reversing does not need Command's
`execute()`/`undo()` ceremony. If a weight correction is rare, low-stakes,
and easy to re-enter by hand if someone gets it wrong, a plain function
call does the job a `ShipmentCommand` subclass would, without a
`CommandHistory` to maintain.

Finally, composing patterns for the sake of demonstrating range is itself
a smell. This chapter's five patterns are each justified by a specific,
present requirement of a shipment dispatch console: a rate rule that
varies by carrier contract, carrier-specific construction, a lifecycle
with illegal transitions to reject, operator actions that need to be
reversible, and more than one subscriber that cares about a status change.
A smaller system that needs only two or three of those things should
compose only those two or three, and a system that needs none of them
should stay a handful of functions. These four seams matter because they
mark where real coupling has to exist between patterns; they are not a
checklist to complete on every project that happens to involve more than
one pattern.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley):
  the canonical description of every pattern this chapter composes,
  including the forces each one answers in isolation before this chapter
  puts them together.
- Robert C. Martin, [*Agile Software Development, Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the SOLID reasoning behind why
  `ShipmentDispatcher` is the only type that depends on all five patterns
  at once, while every other component in this chapter's system depends
  on exactly one abstraction and nothing further.
