---
title: "State and finite state machines"
order: 10
part: "selecting-varying-behavior"
description: "A shipment's lifecycle modeled four ways: the GoF State pattern with one type per status, a data-driven enum-plus-transition-table machine, a std::variant dispatched with std::visit, and a probability-driven machine whose weighted transitions are made reproducible by injecting a seeded random engine."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Build the GoF State pattern: one type per status, a context that owns the current state through a pointer, and a default-reject contract so illegal events fail loudly instead of silently.
- Build the same lifecycle as a data-driven finite state machine: a literal transition table a reviewer can read as data, with no per-status branching code anywhere.
- Build the same lifecycle again as a `std::variant` of state types, dispatched with `std::visit` and an overloaded-lambda set, and say what that buys you over a class hierarchy.
- Build a probability-driven state machine whose transitions are chosen by weighted draws from `<random>`, and make that machine's behavior reproducible in a test by injecting a seeded engine rather than owning a global one.
- Choose among all four when a status change needs validating, one of them needs a typed rejection reason, and say which standard each answer belongs to.
- Recognize when any of these four is solving a problem a plain `if` chain already solves, and say what to write instead.

## Why this matters

An order moves through a small, fixed set of statuses on its way from checkout to a customer's door: it is created, paid for, handed to a carrier, and delivered, with a cancellation possible before it ships. Chapter 1 called this kind of force a varying behavior tied to a changing condition rather than to a changing algorithm: the question is never "which pricing rule applies," the question this chapter trains for is "which events are legal right now, and what happens next." That is the State pattern's whole claim, the Gang of Four's answer to an object whose behavior depends on which of a known, finite set of conditions it is currently in, and it is also the shape every finite state machine shares whether or not a GoF class hierarchy sits underneath it.

What earns this force a full chapter is the same thing Chapter 8 found for Strategy: modern C++ gives more than one idiomatic way to express it, and the differences are not cosmetic. The GoF State pattern puts the rules inside the types themselves, so a reviewer reading `PaidState` sees exactly what a paid shipment can legally do next without reading a single `if`. A data-driven transition table puts the same rules in one literal structure instead, so a reviewer checking "can a shipment be cancelled after it ships" reads one row, or its absence, rather than tracing a virtual call. A `std::variant` dispatched with `std::visit` gets the type-per-status clarity of the GoF version without a base class, a `unique_ptr`, or a single heap allocation anywhere in the machine. A fourth expression breaks from all three: a shipment that is in transit does not move to its next status because a caller asked it to, it moves because a weighted die roll, backed by a real probability distribution, decided the outcome.

{% include excalidraw.html name="10-state-transitions" caption="Figure 10.1: A shipment's legal states and transitions, plus the weighted-probability outlet the stochastic machine draws from out of InTransit." %}

## The domain: a shipment's lifecycle

The example behind this chapter lives in `examples/10-state-machines`. A shipment carries one `ShipmentStatus` at a time out of six: `Created`, `Paid`, `InTransit`, `Delivered`, `Cancelled`, and `Lost`. The first five are the legal lifecycle every expression in this chapter can reach by calling an event; `Lost` exists only as an outcome the stochastic machine can draw, the same weighted outlet the diagram above calls out on `InTransit`. Four events move a shipment between statuses: `Pay`, `Ship`, `Deliver`, and `Cancel`. `shipment_domain.hpp` declares both enums plus one exception type, `IllegalTransition`, that every expression below throws the same way when an event does not apply to the current status:

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

That shared vocabulary is the only thing the four expressions below have in common. Unlike Chapter 8's three Strategy expressions, which all called down into one shared pricing function, the GoF state machine, the transition table, and the `std::variant` machine each encode the legal moves independently, in their own idiom. The reason is the thing under test: this chapter's suite proves that three different representations of the same rule set agree, which only means something if each one states the rules itself rather than delegating to a fourth, canonical function.

## Expression one: the GoF State pattern

`ShipmentState` is the interface every status implements. Every event method defaults to rejecting the call; a concrete state overrides only the events it accepts:

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

`reject()` throws `IllegalTransition(status(), event)`, so a state that never overrides `ship()` rejects it automatically, with no copy-pasted error handling in every concrete class. `CreatedState` overrides `pay()` to return a `PaidState` and `cancel()` to return a `CancelledState`; `PaidState` overrides `ship()` and `cancel()`; `InTransitState` overrides only `deliver()`; `DeliveredState` and `CancelledState` override nothing at all, which is exactly correct, since every event is illegal once a shipment is in either terminal status.

`GofStateMachine` is the context object, and it is almost nothing but plumbing:

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

The order of operations in each one-liner is what makes this version exception-safe without a single line of explicit handling. `state_->pay()` either returns a new state or throws, and the throw happens before `state_` is ever reassigned, so a rejected event leaves the machine exactly where it was. This is the same `unique_ptr`-owns-polymorphism move Chapter 8 used for `RateEstimator`: `GofStateMachine` owns its current state outright, nothing else can share it, and swapping it in `pay()` runs the old state's destructor with no explicit `delete` anywhere in this example. A reader who wants to know what a `PaidState` can do next opens exactly one class and reads two overrides; the cost is a virtual call per event and a heap allocation per transition, the same trade Chapter 8 priced for runtime polymorphism.

## Expression two: two idiomatic finite state machines

### A data-driven transition table

The GoF version puts the rule "a paid shipment can be shipped or cancelled" inside `PaidState`'s source code. A transition table puts the identical rule in a value instead:

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

`TableShipmentMachine::apply()` does nothing but look up `(status_, event)` and throw `IllegalTransition` when the lookup misses:

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

Every legal transition in this machine is a single line in one literal, in one place, with no class to open and no virtual call to trace. That is the real argument for a data-driven FSM over a class-per-state one: a product manager asking "what happens when a cancelled order gets a delivery event" can answer the question by scanning five rows, something a GoF hierarchy makes the reader trace through a type they first have to locate. The cost moves the other way too: adding a brand-new status means touching this one table and nothing else, but the table has no natural place to attach status-specific behavior beyond "what status comes next," the way `InTransitState::deliver()` could, in principle, also log a delivery timestamp or fire an event. A table is the right shape when the only thing that varies by status is which transitions are legal; a GoF hierarchy earns its extra ceremony when different statuses need meaningfully different behavior beyond that.

### A `std::variant` dispatched with `std::visit`

The second idiomatic FSM keeps the one-type-per-status clarity of the GoF version without a base class anywhere. Each status is an empty tag type, and `ShipmentVariant` is a `std::variant` over all five legal tags:

```cpp
struct CreatedTag {};
struct PaidTag {};
struct InTransitTag {};
struct DeliveredTag {};
struct CancelledTag {};

using ShipmentVariant = std::variant<CreatedTag, PaidTag, InTransitTag, DeliveredTag, CancelledTag>;
```

`apply_event()` pattern-matches on the active alternative using the standard overloaded-lambda set, one callable per tag type, so `std::visit` picks the right one at compile time instead of a chain of `if constexpr`:

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

This machine never allocates on the heap, never calls through a vtable, and never owns a pointer: `ShipmentVariant` is a value, copyable and comparable the way any value is, and the compiler knows its exact size and every alternative it can ever hold, because the set of states is closed at compile time. That closed-set guarantee is `std::variant`'s real advantage over the GoF hierarchy's open one: a GoF `ShipmentState` subclass can be added anywhere in the program that includes the header, which is exactly the extensibility a plugin architecture wants and exactly the thing a reviewer checking "are these the only five statuses that exist" cannot rule out by reading one file. `ShipmentVariant`'s alternative list is that file, in its entirety, and `std::visit` with an exhaustive `Overloaded` set will not compile if a new tag type is added to the `variant` without a matching lambda, turning a missed case into a compiler error instead of a silent default.

## Expression three: a probability-driven state machine

The first three expressions all share one property: a caller supplies the event, and the machine either accepts or rejects it. `StochasticShipmentMachine` breaks that contract for exactly one transition. Once a shipment is `InTransit`, nothing calls `deliver()` on it; instead, `advance()` draws a weighted outcome from an injected random engine:

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

`pay()`, `ship()`, and `cancel()` enforce the same legal moves as the table FSM above, deterministically. `advance()` is the new piece, and the weights it draws from are plain data, the same three numbers the diagram calls out on `InTransit`:

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

Eighty-five times in a hundred, the draw lands on `Delivered` and the shipment reaches its terminal status. Ten times in a hundred, the draw lands back on `InTransit` itself, modeling a carrier delay with no status change at all. Five times in a hundred, the draw lands on `Lost`, a terminal status the deterministic expressions above never produce, because nothing in their event vocabulary represents a carrier losing a parcel. `run_to_terminal()` calls `advance()` in a loop until the status stops being `InTransit`, collecting every status observed along the way, which is what turns a single weighted draw into a self-looping Markov chain: a shipment can be delayed zero, one, or several times before it finally resolves.

The detail that makes this machine testable at all is where the random engine lives. `std::mt19937 rng_` is a constructor parameter, not a static engine the class owns internally and not a call to `std::random_device` buried inside `advance()`. A production caller constructs the machine with `std::mt19937{std::random_device{}()}`, a real, unpredictable seed; this chapter's GoogleTest suite constructs it with `std::mt19937(38)`, a fixed seed chosen because, under this repository's toolchain, it produces a short, illustrative run: three delays followed by a delivery. Seeding the same engine with the same value twice produces the exact same sequence of draws, because `std::mt19937` is a deterministic pseudo-random generator; the randomness a caller sees comes entirely from the seed, and a test that controls the seed controls the output. Nothing about `StochasticShipmentMachine` itself changed to make this possible, no test-only branch, no mock, no `#ifdef`; injecting the engine through the constructor was the whole design decision.

{% include excalidraw.html name="10-fsm-representations" caption="Figure 10.2: the same shipment rules as a GoF hierarchy, a transition table, a std::variant, and a stochastic machine that draws its own next state." %}

## Typed rejection: C++20 versus C++23

Every expression above reports an illegal event by throwing `IllegalTransition`, which works, but it forces every caller to either wrap each call in a `try` block or accept that an unhandled exception can propagate out of a status change on a checkout path. C++23's `std::expected` gives `TableShipmentMachine` a second entry point that reports the same rejection without throwing at all:

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

Both columns reject the exact same `(status, event)` pairs; only the shape of the rejection changes between standards, the same pattern Chapters 3 and 8 used for an invalid invoice and a malformed shipment. `apply()` keeps throwing in both standards, since the GoF machine, the `std::variant` machine, and `apply()` itself all rely on the same exception contract for the cross-check tests that prove the three deterministic expressions agree; `try_apply()` is an additional, non-throwing entry point on the table machine alone, not a replacement for the shared contract the other expressions depend on.

> **AI sidebar.** Ask an LLM pairing partner for "a state machine in C++" and the overwhelmingly common answer is the GoF shape: an abstract base class, one subclass per status, a context holding a pointer. That is a correct answer to one specific force, not the only answer to the general one. Check what the model hands back against the three questions this chapter trained: does the model's design need per-status behavior beyond "what status comes next," in which case the GoF shape or a `std::variant` fits better; does it need the rule set readable as data by someone who is not a C++ reader, in which case the transition table is the better fit; and does any part of the transition depend on a condition the caller cannot observe in advance, a network timeout, a carrier's delivery outcome, a feature flag's sampled rollout? A model asked to "make shipment delivery more realistic" will often reach for a `sleep()` call or a hardcoded delay to fake unpredictability, which looks dynamic but is not probabilistic at all, no weights, no distribution, and no way to tune how often a delay should occur. If the unpredictability needs to be quantified, weighted, and reproducible in a test, a `std::discrete_distribution` over an injected, seedable engine is what to ask for instead, and a generated answer that owns its own `std::mt19937` as a private member with no way to seed it from outside is the clearest sign the model wrote code that runs but cannot be tested.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/10-state-machines` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 31 cases under the `cpp23` preset, 29 under `cpp20`, the difference being the two `std::expected`-specific cases on `TableShipmentMachine::try_apply()` that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 31   # cpp23
100% tests passed, 0 tests failed out of 29   # cpp20
```

Among those cases, `CrossCheckTest` drives the same event sequence through the GoF machine, the transition table, and the `std::variant` machine side by side and asserts all three land on the same status after every event, and `StochasticShipmentMachineTest.SeededRunProducesAnExactReproducibleSequence` asserts the stochastic machine's output for a fixed seed against an exact, hardcoded sequence rather than merely checking that it terminates.

To see all four expressions walk the same lifecycle rather than just reading an assertion that says they agree, run the example's own demo script:

```sh
examples/10-state-machines/demo.sh cpp23
```

It drives `Pay` -> `Ship` -> `Deliver` through the GoF machine, the table, and the variant, shows the C++23 typed rejection, and then runs the stochastic machine from a fixed seed of 38:

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

Notice that all three deterministic expressions land on `Delivered` for the identical `Pay`/`Ship`/`Deliver` sequence, the observable version of the cross-check test above. The stochastic block shows seed 38 taking three delays before resolving; running `examples/10-state-machines/demo.sh cpp20` prints the same output minus the `std::expected` block, which only compiles under the `cpp23` preset, with the stochastic sequence identical in both, because `std::mt19937` behaves the same way under either standard.

## When not to use any of these

All four expressions in this chapter earn their cost when a thing has more than a couple of statuses, when the legal transitions between them are a real constraint worth enforcing rather than a formality, and when illegal events are a case the code must guard against. They stop paying for themselves well before that.

A status with two values and one transition between them does not need a `ShipmentState` hierarchy, a transition table, or a `std::variant`; it needs a `bool`, or at most a two-value `enum class` checked with a plain `if`. Wrapping a flag that only ever flips from `false` to `true` in any of this chapter's machinery adds a type, a header, and a test file with nothing underneath it to justify the ceremony. The same caution applies in the other direction, to events a caller can never issue illegally: if the only code path that ever calls `ship()` already checked that the shipment was paid moments earlier, in the same function, enforcing that same rule a second time inside the state machine is not wrong, but it is not buying anything either, and a reviewer who sees `IllegalTransition` guards on every event may reasonably ask where the illegal calls are expected to come from.

The stochastic expression needs an extra line of scrutiny beyond the other three, because it is the easiest of the four to reach for out of habit rather than need. A transition with real uncertainty in the world behind it, a carrier's delivery outcome, a flaky network call's retry result, a feature rollout's sampled cohort, is what `std::discrete_distribution` and an injected engine are for. A transition that is deterministic but inconvenient to model precisely, "sometimes shipment status updates arrive late from the carrier's API," is not the same thing, and modeling a data problem as a probability distribution can hide a fixable bug in a carrier integration behind a parameter that looks intentional. Reach for the weighted draw when the uncertainty is real and worth quantifying; reach for a retry, a timeout, or a bug report when it is not.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original State pattern entry, including the intent statement this chapter's GoF expression satisfies directly and the other three expressions satisfy in different idioms.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/10-state-machines`'s suite, which asserts that three deterministic expressions agree on every step of a shared event sequence and that a seeded stochastic run reproduces an exact, hardcoded sequence rather than merely completing without error.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
