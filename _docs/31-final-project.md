---
title: "Final project: composing patterns into a system"
order: 31
part: "putting-it-together"
description: "A shipment dispatch console that composes Strategy, Factory, State, Command and Observer into one system, taught at the seams where the patterns meet rather than re-teaching any one of them."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Compose Strategy, Factory, State, Command and Observer into one running system, and name the object that owns each role.
- Identify the seams where two patterns meet: Factory handing back a Strategy, Command driving State, Command invoking Strategy, and State firing Observer.
- Design one composition root that wires every pattern without any collaborator knowing about the others.
- Write an integration test that drives the composed system end to end: a computed rate, fired notifications and a reversed correction.
- Recognize when composing this many patterns over-engineers a smaller problem, and what a smaller system should do instead.

{% include excalidraw.html name="31-final-project" caption="Figure 31.1: ShipmentDispatcher as the composition root, with Factory handing back a Strategy-bearing CarrierProfile, Command driving both State and Strategy at their seams, and State notifying Observer on every transition." %}

## Why this matters

Each pattern so far came with one force, one example and its own tests. That's how to learn what Strategy buys or what Command's undo costs. It isn't how patterns appear in a real system. A shipment dispatch console has one `Shipment` whose lifecycle fires events something else reacts to, one rate calculation invoked on demand, and one carrier-selection step that hands the right pieces downstream. The patterns meet at seams, and the hard part is the moment two of them cooperate inside one method.

The final project composes five patterns into one `ShipmentDispatcher` a warehouse operator's console could run:

| Pattern | Role in the dispatcher |
| --- | --- |
| Strategy | Rate selection |
| Factory | Carrier creation |
| State | The shipment lifecycle |
| Command | Reversible operator actions |
| Observer | Lifecycle notifications |

It assumes you know each pattern by name and spends its words on the four places they touch.

## The domain: a shipment dispatch console

The system lives in `examples/31-final-project`. A `Shipment` carries an id, a carrier name, a destination zone, a weight in kilograms and a quoted rate in cents, plus a status: `Created`, `PickedUp`, `InTransit` or `Delivered`. Three events, `PickUp`, `Dispatch` and `Deliver`, move it forward one step at a time, and `Shipment::advance()` enforces the order:

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

That's the whole State component: a context that owns its status, rejects an event that doesn't apply, and calls `notify()` only after a transition succeeds. `Shipment` is also the Observer subject, holding a list of `LifecycleObserver*` subscribers and fanning each accepted transition out to all of them. Both roles sit on one type so the State machine's success and the Observer's trigger are the same event, with nothing between them to drift out of sync.

## Seam one: Factory hands back a Strategy

`CarrierFactory::create()` takes a `CarrierRequest`, a carrier code and a destination zone, and returns a `CarrierProfile`: a carrier name, a service-level commitment in hours, and a `std::unique_ptr<RateStrategy>` already pointing at the right concrete strategy.

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

No caller names `WeightTieredRateStrategy` or `FlatRateStrategy`. `ShipmentDispatcher::open_shipment()` asks for a carrier by code and immediately asks the profile's `rate_strategy` for the opening quote:

```cpp
carrier_profile_ = factory_.create(request);
shipment_.emplace(std::move(shipment_id), carrier_profile_->carrier_name,
                   request.destination_zone, weight_kg, Cents{0});
shipment_->set_quoted_rate_cents(carrier_profile_->rate_strategy->quote(*shipment_));
```

The Factory chooses which Strategy a caller gets, and the caller never learns a choice was made. A carrier with a third pricing rule needs one new `RateStrategy` subclass and one new branch in `CarrierFactory::create()`. `ShipmentDispatcher` doesn't change, because it was never written against a concrete strategy.

## Seam two: Command drives State

`AdvanceLifecycleCommand` wraps one call to `Shipment::advance()`, recording the status first so `undo()` can restore it:

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

Undo doesn't fire a reverse event. Carriers don't support an "un-pick-up", and neither does this machine. `restore_status_for_undo()` is a narrower entry point that skips the transition table and sets the status back: capture a snapshot, restore it, the usual Command technique for an action with no clean inverse. The restore still notifies every Observer, because an operator correcting a mis-scanned pickup is an event a dashboard should see.

`ShipmentDispatcher::make_advance_command()` is the seam at the call site:

```cpp
std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_advance_command(ShipmentEvent event) {
    return std::make_unique<AdvanceLifecycleCommand>(shipment(), event);
}
```

The console reaches `advance()` only through a command the dispatcher builds and its `CommandHistory` runs. That makes every lifecycle change undoable by construction, not only the ones someone remembered to wrap.

## Seam three: Command invokes Strategy

A warehouse rescale corrects a shipment's weight after it was quoted. `AdjustWeightCommand::execute()` sets the new weight and asks the same `RateStrategy` the carrier was built with for a fresh quote:

```cpp
void AdjustWeightCommand::execute() {
    previous_weight_kg_ = shipment_.weight_kg();
    previous_quoted_rate_cents_ = shipment_.quoted_rate_cents();

    shipment_.set_weight_kg(new_weight_kg_);
    shipment_.set_quoted_rate_cents(rate_strategy_.quote(shipment_));
    executed_ = true;
}
```

`undo()` doesn't call the strategy again. It restores the captured weight and quote. That's cheaper, and it doesn't assume the strategy is a pure function of the shipment's fields, which holds here but might not for every future `RateStrategy`.

`ShipmentDispatcher::make_adjust_weight_command()` hands the command the strategy object the factory built for this carrier:

```cpp
std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_adjust_weight_command(double new_weight_kg) {
    return std::make_unique<AdjustWeightCommand>(shipment(), *carrier_profile().rate_strategy, new_weight_kg);
}
```

A weight correction and the opening quote are always priced by the same rule, because they use the same `RateStrategy` instance.

## Seam four: State fires Observer

A legal transition in `Shipment::advance()` calls `notify()`, which calls `on_transitioned()` on every subscribed `LifecycleObserver`:

```cpp
void Shipment::notify(ShipmentStatus from, ShipmentStatus to) {
    for (LifecycleObserver* observer : observers_) {
        observer->on_transitioned(*this, from, to);
    }
}
```

The demo's `ConsolePrintingObserver` prints a line. A deployment might wire the same interface to a customer notification service, a tracking dashboard and a billing ledger, none referencing the others or the `AdvanceLifecycleCommand` three calls up the stack. An observer knows only that a `Shipment` changed status. So the Command and State seam can change without regard to how many subscribers listen or what they do.

## The composition root

`ShipmentDispatcher` is the only type that knows all five patterns. Every other component sees only the interface next to it: `CarrierFactory` doesn't know `Shipment`, `AdjustWeightCommand` doesn't know `CarrierFactory`, and observers don't know `ShipmentCommand` exists. In the dispatcher, `open_shipment()` calls the factory, `make_advance_command()` and `make_adjust_weight_command()` wire a command to the shipment and, where needed, the factory-built strategy, and `apply()` and `undo_last()` run every command through a `CommandHistory`.

## Factory's rejection path: C++20 versus C++23

`CarrierFactory` is the one component with a C++20/23 difference: how it reports an unknown carrier code. The choice of failure reporting doesn't change which `RateStrategy` a caller gets.

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

> **🤖 Explore with your agent**
>
> "In `examples/31-final-project`, add a carrier code `"PARCELPOST"` to `CarrierFactory::create()` returning a `CarrierProfile` with `carrier_name` "Parcel Post", `sla_hours` 96 and a `FlatRateStrategy` of 650 cents. Don't edit `ShipmentDispatcher`, `AdjustWeightCommand`, `AdvanceLifecycleCommand` or any `LifecycleObserver`. Keep `CarrierProfile` returned by value with its `std::unique_ptr<RateStrategy>`, and keep `AdjustWeightCommand` holding a `const RateStrategy&`. Add a `CarrierFactoryTest` case for the new code, and keep `try_create()` reporting `CarrierFactoryError::UnknownCarrierCode` for unknown codes."
>
> Check with `ctest --preset cpp23 -R 'CarrierFactory|ShipmentDispatcherIntegration'`, and confirm the diff touches only `carrier_factory.cpp` and `tests/carrier_factory_test.cpp`.

## Build, run, observe

From the repository root, the superbuild picks up `examples/31-final-project`:

{% include excalidraw.html name="31-final-project-sequence" caption="Figure 31.2: The time-ordered sequence of one shipment's run, with the same RateStrategy instance quoting the opening rate at step 2 and the corrected rate at step 5." %}

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 34 tests pass under `cpp23` and all 32 under `cpp20`; the difference is `CarrierFactory::try_create()`, which exists only under C++23. Six of the seven test files each cover one component's own contract: lifecycle transitions, the two rate strategies, the factory lookup, the command history and each command's execute and undo. The integration file drives the whole dispatcher through one run: a factory-built strategy prices the opening quote, two lifecycle commands fire notifications in order, a weight correction re-quotes through the same strategy, and undoing it restores the weight and the quote exactly.

To watch the composition run:

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

Each line traces to a seam: the opening quote to Factory handing back Strategy, the `[observer]` lines to State firing Observer, the corrected and restored quotes to Command invoking Strategy and snapshotting around it, and the final rejection to State's transition table. The `cpp20` demo prints the same output.

## When not to use it

Composing five patterns is a cost, not a default. Each pattern needs its own present requirement:

| If the system... | Then drop |
| --- | --- |
| Never changes status, never undoes, prices against one carrier | `ShipmentDispatcher` entirely; a function from weight and carrier code to cents does the job |
| Has exactly one carrier | Factory; construct the one `RateStrategy` directly |
| Never reverses an operation, and a mistaken correction is easy to re-enter | Command and `CommandHistory`; call a plain function |
| Has one subscriber, or none, for status changes | Observer; call the one consumer directly |

Wrapping a one-carrier price function in Strategy, Factory and a composition root is speculative generality spread across five classes. Composing patterns to show range is a smell. Here each pattern answers a present requirement of a dispatch console: a rate rule that varies by carrier contract, carrier-specific construction, a lifecycle with illegal transitions, reversible operator actions, and more than one subscriber to a status change. A system that needs two of those should compose two. The four seams mark where coupling between patterns has to exist; they aren't a checklist for every project that uses more than one pattern.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the canonical description of each composed pattern and the forces it answers on its own.
- Robert C. Martin, [*Agile Software Development, Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the SOLID reasoning behind `ShipmentDispatcher` being the only type that depends on all five patterns while every other component depends on one abstraction.
