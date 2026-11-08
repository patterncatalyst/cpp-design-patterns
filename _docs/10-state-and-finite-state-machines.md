---
title: "State and finite state machines"
order: 10
part: "selecting-varying-behavior"
description: "A shipment's lifecycle modeled four ways: the GoF State pattern with one type per status, a data-driven enum-plus-transition-table machine, a std::variant dispatched with std::visit, and a probability-driven machine whose weighted transitions are made reproducible by injecting a seeded random engine."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Build the GoF State pattern: one type per status, a context that owns the current state through a pointer, and a default-reject contract so illegal events fail loudly.
- Build the same lifecycle as a data-driven finite state machine: a literal transition table a reviewer reads as data.
- Build it again as a `std::variant` of state types dispatched with `std::visit` and an overloaded-lambda set, and say what that buys over a class hierarchy.
- Build a probability-driven state machine whose transitions are weighted draws from `<random>`, made reproducible by injecting a seeded engine.
- Report an illegal event with an exception or with `std::expected`, new in C++23.
- Recognize when any of these solves a problem a plain `if` already solves, and say what to write instead.

## Why this matters

An order moves through a small, fixed set of statuses from checkout to the customer's door: created, paid, handed to a carrier, delivered, with cancellation possible before it ships. What varies isn't an algorithm, as with Strategy, but which events are legal right now and what comes next. That's the State pattern's subject, and every finite state machine shares it, whether or not a GoF hierarchy sits underneath.

{% include excalidraw.html name="10-state-transitions" caption="Figure 10.1: A shipment's legal states and transitions, plus the weighted-probability outlet the stochastic machine draws from out of InTransit." %}

## Pattern: State

### The problem

A shipment holds one `ShipmentStatus` of six: `Created`, `Paid`, `InTransit`, `Delivered`, `Cancelled` and `Lost`. Four events move it: `Pay`, `Ship`, `Deliver` and `Cancel`. `Lost` is reachable only as an outcome the stochastic machine draws. Every expression below throws the same `IllegalTransition` when an event doesn't apply:

```cpp
enum class ShipmentStatus {
    Created, Paid, InTransit, Delivered, Cancelled, Lost,
};

enum class ShipmentEvent {
    Pay, Ship, Deliver, Cancel, Advance,
};

class IllegalTransition : public std::logic_error {
public:
    IllegalTransition(ShipmentStatus from, ShipmentEvent event);
};
```

Written as a `switch` on status inside each event method, the rules scatter across four functions. Nobody can answer "can a shipment be cancelled after it ships?" without reading all of them, and a status added later is easy to miss in one.

### The solution

The **State** pattern lets an object change its behavior when its internal status changes, by giving each status its own representation of which events it accepts and where each leads.

| Expression | Rules live in | Per-event cost | Pick it when |
| --- | --- | --- | --- |
| GoF State hierarchy | One class per status | Virtual call, heap allocation per transition | Statuses need different behavior beyond "what comes next" |
| Transition table | One literal of `(status, event)` rows | Hash lookup | The only thing that varies is which transitions are legal |
| `std::variant` + `std::visit` | One lambda per status | No heap, no vtable | The set of statuses is closed and you want the compiler to check it |
| Stochastic machine | A weight array | One draw | An outcome is uncertain in the world and you want it quantified |

The GoF, table and variant machines each encode the rules themselves rather than calling a shared function, so three independent statements of the same rule set can be checked against each other. They agree on every step of a shared event sequence.

### How C++ models it

`ShipmentState` is the interface every status implements. Every event defaults to rejecting the call; a concrete state overrides only the events it accepts:

```cpp
class ShipmentState {
public:
    virtual ~ShipmentState() = default;
    virtual ShipmentStatus status() const = 0;

    virtual std::unique_ptr<ShipmentState> pay() const { reject(ShipmentEvent::Pay); }
    virtual std::unique_ptr<ShipmentState> ship() const { reject(ShipmentEvent::Ship); }
    virtual std::unique_ptr<ShipmentState> deliver() const { reject(ShipmentEvent::Deliver); }
    virtual std::unique_ptr<ShipmentState> cancel() const { reject(ShipmentEvent::Cancel); }

protected:
    [[noreturn]] void reject(ShipmentEvent event) const;
};
```

`reject()` throws `IllegalTransition(status(), event)`, so nothing repeats the error handling. `CreatedState` overrides `pay()` and `cancel()`, `PaidState` overrides `ship()` and `cancel()`, `InTransitState` overrides only `deliver()`, and `DeliveredState` and `CancelledState` override nothing, because every event is illegal in a terminal status.

`GofStateMachine`, the context, is plumbing:

```cpp
class GofStateMachine {
public:
    GofStateMachine();

    ShipmentStatus status() const { return state_->status(); }

    void pay() { state_ = state_->pay(); }
    void ship() { state_ = state_->ship(); }
    void deliver() { state_ = state_->deliver(); }
    void cancel() { state_ = state_->cancel(); }

private:
    std::unique_ptr<ShipmentState> state_;
};
```

Each one-liner is exception-safe without any handling. `state_->pay()` returns a new state or throws, and the throw comes before `state_` is reassigned, so a rejected event leaves the machine where it was. When `pay()` succeeds, the `unique_ptr` destroys the old state. A reader who wants to know what a paid shipment can do opens `PaidState` and reads two overrides.

### A transition table

A table puts the same rules in a value:

{% raw %}
```cpp
using ShipmentTransitionTable =
    std::unordered_map<ShipmentTransitionKey, ShipmentStatus, ShipmentTransitionKeyHash>;

const ShipmentTransitionTable& shipment_transition_table() {
    static const ShipmentTransitionTable table{
        {{ShipmentStatus::Created, ShipmentEvent::Pay}, ShipmentStatus::Paid},
        {{ShipmentStatus::Created, ShipmentEvent::Cancel}, ShipmentStatus::Cancelled},
        {{ShipmentStatus::Paid, ShipmentEvent::Ship}, ShipmentStatus::InTransit},
        {{ShipmentStatus::Paid, ShipmentEvent::Cancel}, ShipmentStatus::Cancelled},
        {{ShipmentStatus::InTransit, ShipmentEvent::Deliver}, ShipmentStatus::Delivered},
    };
    return table;
}
```
{% endraw %}

`TableShipmentMachine::apply()` looks up `(status_, event)` and throws when the lookup misses:

```cpp
void TableShipmentMachine::apply(ShipmentEvent event) {
    const auto& table = shipment_transition_table();
    const auto it = table.find(ShipmentTransitionKey{status_, event});
    if (it == table.end()) {
        throw IllegalTransition(status_, event);
    }
    status_ = it->second;
}
```

Every legal transition is one row. A product manager asking "what happens when a cancelled order gets a delivery event?" scans five rows and sees there isn't one. The trade-off: the table holds only "which status comes next". It has no place for status-specific behavior, such as `InTransitState::deliver()` also recording a timestamp. Use a table when legality is the only thing that varies, and a hierarchy when statuses behave differently.

### A std::variant dispatched with std::visit

The variant keeps one type per status with no base class. Each status is an empty tag, and `ShipmentVariant` holds one of the five legal tags:

```cpp
struct CreatedTag {};
struct PaidTag {};
struct InTransitTag {};
struct DeliveredTag {};
struct CancelledTag {};

using ShipmentVariant = std::variant<CreatedTag, PaidTag, InTransitTag, DeliveredTag, CancelledTag>;
```

`apply_event()` dispatches on the active alternative with an overloaded-lambda set, one callable per tag:

```cpp
ShipmentVariant apply_event(const ShipmentVariant& state, ShipmentEvent event) {
    return std::visit(
        Overloaded{
            [event](CreatedTag) -> ShipmentVariant {
                if (event == ShipmentEvent::Pay) return PaidTag{};
                if (event == ShipmentEvent::Cancel) return CancelledTag{};
                throw IllegalTransition(ShipmentStatus::Created, event);
            },
            [event](PaidTag) -> ShipmentVariant {
                if (event == ShipmentEvent::Ship) return InTransitTag{};
                if (event == ShipmentEvent::Cancel) return CancelledTag{};
                throw IllegalTransition(ShipmentStatus::Paid, event);
            },
            // InTransitTag, DeliveredTag, CancelledTag follow the same shape
        },
        state);
}
```

The machine never allocates, never calls through a vtable and owns no pointer. `ShipmentVariant` is a value with a known size and a closed set of alternatives. A GoF hierarchy is open: any file that includes the header can add a `ShipmentState` subclass, which a plugin design wants and a reviewer checking "are these the only statuses?" can't rule out. The variant's alternative list is the whole answer. Add a tag to the variant without a matching lambda and `std::visit` fails to compile with `no type named 'type' in 'std::invoke_result<...>'`, so a missed case is a build error, not a silent default.

### A probability-driven state machine

The first three machines accept or reject an event a caller supplies. `StochasticShipmentMachine` breaks that for one transition: once a shipment is `InTransit`, nothing calls `deliver()`. `advance()` draws the outcome from an injected engine:

```cpp
class StochasticShipmentMachine {
public:
    explicit StochasticShipmentMachine(std::mt19937 rng, ShipmentStatus initial = ShipmentStatus::Created);

    ShipmentStatus status() const { return status_; }

    void pay();
    void ship();
    void cancel();

    ShipmentStatus advance();
    std::vector<ShipmentStatus> run_to_terminal(int max_steps = 10'000);

private:
    ShipmentStatus status_;
    std::mt19937 rng_;
};
```

`pay()`, `ship()` and `cancel()` enforce the table's rules deterministically. The weights for `advance()` are data:

```cpp
constexpr std::array<double, 3> kInTransitWeights = {0.85, 0.10, 0.05};

ShipmentStatus StochasticShipmentMachine::advance() {
    if (status_ != ShipmentStatus::InTransit) {
        throw IllegalTransition(status_, ShipmentEvent::Advance);
    }
    std::discrete_distribution<int> distribution(kInTransitWeights.begin(), kInTransitWeights.end());
    const int index = distribution(rng_);
    status_ = outcome_for_index(index);
    return status_;
}
```

| Draw | Weight | Outcome |
| --- | --- | --- |
| `Delivered` | 0.85 | Terminal |
| `InTransit` | 0.10 | A carrier delay; the status doesn't change |
| `Lost` | 0.05 | Terminal; no deterministic event produces it |

`run_to_terminal()` calls `advance()` until the status leaves `InTransit` and records each status seen. That makes a self-looping Markov chain: a shipment can be delayed any number of times before it resolves.

The engine is a constructor parameter, not a static the class owns and not a `std::random_device` inside `advance()`. Production code passes `std::mt19937{std::random_device{}()}`. A test passes `std::mt19937(38)`, which yields three delays and then a delivery. `std::mt19937` is a deterministic generator, so the same seed gives the same draws, and a check can pin the exact sequence. Nothing in the class changed to allow that: no test-only branch, no mock, no `#ifdef`.

{% include excalidraw.html name="10-fsm-representations" caption="Figure 10.2: the same shipment rules as a GoF hierarchy, a transition table, a std::variant, and a stochastic machine that draws its own next state." %}

### Typed rejection

Every machine reports an illegal event by throwing `IllegalTransition`, so a caller on a checkout path either wraps each call in `try` or lets the exception escape. `std::expected`, new in C++23, gives `TableShipmentMachine` a second entry point, `try_apply()`, that returns the rejection as a value:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: no equivalent entry point exists in the mandatory build path. A
// caller that needs to avoid a thrown exception wraps apply() in its own
// try block; the thrown IllegalTransition already carries the full reason
// in its what(), but only a catch block can read it.
try {
    machine.apply(ShipmentEvent::Ship);
} catch (const IllegalTransition& rejected) {
    // handle rejected.what()
}
```
```cpp
// C++23: std::expected carries the same rejection reason as a typed
// return value, checkable with an if and no exception machinery. This is
// the version TableShipmentMachine actually ships, guarded behind
// #if __cplusplus >= 202302L so the mandatory C++20 build path never
// needs std::expected.
std::expected<ShipmentStatus, std::string> try_apply(ShipmentEvent event) {
    const auto& table = shipment_transition_table();
    const auto it = table.find(ShipmentTransitionKey{status_, event});
    if (it == table.end()) {
        return std::unexpected<std::string>(/* same rejection message */);
    }
    status_ = it->second;
    return status_;
}
```

Both reject the same `(status, event)` pairs. `apply()` keeps throwing under both standards, because the GoF and variant machines share that contract. `try_apply()` is an extra, non-throwing entry point on the table machine only.

## When not to use it

- **Two statuses and one transition.** Use a `bool`, or a two-value `enum class` checked with an `if`. A hierarchy, table or variant adds a type, a header and a test file around a flag.
- **Events that can't arrive illegally.** If the only caller of `ship()` checked the payment a few lines earlier, a second guard in the machine buys nothing, and a reviewer will ask where illegal calls are expected from.
- **Uncertainty that isn't real.** A carrier's delivery outcome or a sampled feature rollout is what a `std::discrete_distribution` over an injected engine is for. "Status updates sometimes arrive late from the carrier's API" is a data problem; modeling it as a probability hides a fixable integration bug behind a parameter that looks intentional. Use a retry, a timeout or a bug report instead.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a state machine.** "Add a customs hold to the shipment state machine" leaves open which of the representations change, whether they must still agree and how an illegal event is reported. State it:

> In `examples/10-state-machines`, add a `HeldAtCustoms` status reachable from `InTransit` by a new `Hold` event and leaving by `Release` back to `InTransit`. Add it to all three deterministic machines: a new `ShipmentState` subclass for `GofStateMachine`, two rows in `shipment_transition_table()`, and a `HeldAtCustomsTag` with its own lambda in every `std::visit` over `ShipmentVariant`. Every other event from `HeldAtCustoms` throws `IllegalTransition`. Don't change `kInTransitWeights` or the `std::mt19937` constructor parameter on `StochasticShipmentMachine`. Add cross-check tests that drive `Pay`, `Ship`, `Hold`, `Release`, `Deliver` through all three machines and expect the same status after each event.

Run `ctest --preset cpp23 -R CrossCheck` and check that the variant still has no default lambda.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/10-state-machines`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 31 cases pass under `cpp23` and 29 under `cpp20`; the two `try_apply()` cases compile only under C++23. To see the four machines walk the lifecycle:

```sh
examples/10-state-machines/demo.sh cpp23
```

It drives `Pay`, `Ship`, `Deliver` through the GoF machine, the table and the variant, shows the typed rejection, then runs the stochastic machine from seed 38:

```
== Expression 1: GoF State pattern ==
Start: Created
After pay(): Paid
After ship(): InTransit
After deliver(): Delivered

== Expression 2a: enum + transition table FSM ==
Same event sequence via the table: Delivered

== Expression 2b: std::variant + std::visit FSM ==
Same event sequence via the variant: Delivered

== C++23 refinement: typed rejection via std::expected ==
Rejected: cannot apply event 'Ship' to a shipment in status 'Created'

== Expression 3: stochastic FSM (seeded for reproducibility) ==
Seed 38, InTransit outcomes: InTransit InTransit InTransit Delivered
Final status: Delivered
```

All three deterministic machines reach `Delivered`, and seed 38 takes three delays before delivery. Under `cpp20` the output omits the `std::expected` block, and the stochastic sequence is the same.

- **Delete a table row.** In a scratch copy, remove the `(Paid, Cancel)` row from `shipment_transition_table()`. Two cases fail: the table now rejects cancelling a paid shipment, and it disagrees with the GoF and variant machines.
- **Add a status to the variant only.** Add a `HeldAtCustomsTag` to `ShipmentVariant` without a lambda for it. The build fails inside `std::visit` with `no type named 'type' in 'std::invoke_result<...>'`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original State pattern entry, whose intent the GoF expression satisfies directly and the other three satisfy in different idioms.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/10-state-machines`, which cross-checks the three deterministic machines and pins a seeded stochastic run to an exact sequence.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
