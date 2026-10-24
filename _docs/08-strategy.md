---
title: "Strategy"
order: 8
part: "selecting-varying-behavior"
description: "Three idiomatic C++ ways to select a varying algorithm at runtime or compile time, taught against a real shipping-rate example: an abstract interface behind a unique_ptr, a std::function value, and a compile-time policy constrained by a concept."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the forces that make Strategy the right candidate pattern for a varying algorithm, and the candidate non-pattern it competes against.
- Build a Strategy with runtime polymorphism: an abstract interface, concrete implementations, and a `std::unique_ptr` the owner can swap at runtime.
- Build the same Strategy as a value-semantic `std::function`, and explain what composing two strategies at runtime buys you that a class hierarchy does not.
- Build the same Strategy as a compile-time policy, constrained by a C++20 concept, with zero runtime dispatch cost.
- Choose between `std::optional`-or-throw error handling and C++23's `std::expected` for a Strategy's failure path, and say which standard each belongs to.
- Recognize when a Strategy is solving a problem nobody has yet, and say what to write instead.

## Why this matters

A checkout flow has to quote a shipping cost, and the rule for computing that cost is not fixed: a merchant might ship everything at a flat rate, price by weight, or run a distance-and-weight tiered table, and the choice is made per carrier contract, not per line of code. That is the shape Chapter 1 called a force: the pricing rule varies, and the rest of the checkout flow around it does not. Strategy is the Gang of Four's name for exactly this force, extracting the algorithm that varies into its own unit and letting the code that uses it depend on what the algorithm does rather than on which one it is.

What makes Strategy worth a full chapter, ahead of every other pattern in this book, is that modern C++ gives you three distinct ways to resolve the same force, each with its own cost and its own guarantee. An abstract interface behind a `std::unique_ptr` buys you a strategy that a running program can swap on the fly, at the price of a virtual call and a heap allocation. A `std::function` buys you the same runtime swappability as a value, with the bonus that two strategies compose into a third one without a decorator class, at the price of type erasure. A template parameter constrained by a concept buys you the same quote with no indirection at all, at the price of fixing the choice before the program ever runs. None of the three is "the" Strategy pattern in this book's catalogue; all three are, and choosing among them is itself the design decision this chapter trains.

{% include excalidraw.html name="08-strategy-forces" caption="Figure 8.1: Strategy's forces resolved three idiomatic ways: runtime polymorphism behind a unique_ptr, a value-semantic std::function, and a compile-time policy constrained by a concept." %}

## The domain: pricing a shipment at checkout

The example behind this chapter lives in `examples/08-strategy`, and its domain is plain by design, so the three Strategy expressions stay the focus: a `Shipment` carries a weight in kilograms, a `DestinationZone` (`Local`, `Regional`, `National`, `International`), and a declared value in cents. Money in this domain is always an integer `Cents`, never a `double`, so a quote is exact and a test assertion can compare it for equality without an epsilon anywhere in sight.

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

The pricing math itself lives in one place, `shipping_domain.cpp`, and every one of the three Strategy expressions in this chapter calls down into it: `flat_rate_cost`, `weight_based_cost`, and `tiered_zone_cost`. This separation matters more than it looks. It means an identical `Shipment` prices identically no matter which dispatch mechanism quoted it, which is the behavior this example's GoogleTest suite actually verifies, not an incidental side effect of three implementations happening to agree. If this book's example mixed the pricing rules into each Strategy expression separately, a bug fixed in one would silently stay broken in the other two; keeping the rate math as pure, shared functions makes that class of bug impossible rather than merely unlikely.

## Expression one: runtime polymorphism

The classic Gang of Four shape is an abstract interface and a family of concrete implementations, selected and held through a pointer the owning object can reassign. `RateStrategy` declares one pure virtual method, `quote`, and three carriers implement it: `FlatRateCarrier`, `WeightBasedCarrier`, and `TieredZoneCarrier`.

```cpp
class RateStrategy {
public:
    virtual ~RateStrategy() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};

class WeightBasedCarrier : public RateStrategy {
public:
    WeightBasedCarrier(Cents base_cents, Cents per_kg_cents);
    Cents quote(const Shipment& shipment) const override;
private:
    Cents base_cents_;
    Cents per_kg_cents_;
};
```

`RateEstimator` is the object that actually uses a carrier. It owns exactly one `RateStrategy` at a time, through a `std::unique_ptr`, injected through the constructor and reassignable later with `use_strategy()`:

```cpp
class RateEstimator {
public:
    explicit RateEstimator(std::unique_ptr<RateStrategy> strategy);
    void use_strategy(std::unique_ptr<RateStrategy> strategy);
    Cents estimate(const Shipment& shipment) const;
private:
    std::unique_ptr<RateStrategy> strategy_;
};
```

The `unique_ptr` is doing real work here, not just holding a pointer. `RateEstimator` owns its carrier outright: nothing else can share it, nothing else can outlive it, and when a caller swaps in a new carrier with `use_strategy()`, the old one's destructor runs automatically, no explicit `delete` anywhere in this example. That ownership guarantee is what Chapter 5 calls RAII applied to polymorphism: the lifetime of the strategy object is tied to the lifetime of the thing that uses it, enforced by the type system rather than by a comment telling the caller to clean up. This is also the expression to reach for when a strategy must be selected from outside the program itself, for instance a carrier name read from a configuration file at startup and mapped to a concrete class through a small factory, because a `unique_ptr<RateStrategy>` can be constructed from any one of several concrete types behind a single, uniform interface.

## Expression two: a value-semantic callable

The second expression keeps the same runtime swappability without an abstract base class anywhere. `RateFn` is just a type alias for `std::function<Cents(const Shipment&)>`; a strategy is any callable with that signature, whether a free function, a lambda with captured state, or the composition of two other `RateFn` values.

```cpp
using RateFn = std::function<Cents(const Shipment&)>;

RateFn weight_based(Cents base_cents, Cents per_kg_cents) {
    return [base_cents, per_kg_cents](const Shipment& shipment) {
        return weight_based_cost(base_cents, per_kg_cents, shipment.weight_kg);
    };
}
```

`QuoteBook` plays the same role `RateEstimator` played above, holding one `RateFn` by value and letting a caller reassign it with `use()`. Holding the strategy by value rather than through a pointer is the real difference from expression one: there is no heap-owned polymorphic object to manage, no virtual destructor to get right, just a value that can be copied, moved, and compared for truthiness like any other `std::function`.

The capability this expression adds, and the reason this book teaches it as a distinct idiom rather than a stylistic variant of expression one, is composition. `with_insurance_surcharge()` takes an existing `RateFn` and returns a new one that adds a surcharge proportional to the shipment's declared value:

```cpp
RateFn with_insurance_surcharge(RateFn base_rate, double cents_per_declared_dollar) {
    return [base_rate = std::move(base_rate), cents_per_declared_dollar](const Shipment& shipment) {
        const Cents base_cost = base_rate(shipment);
        const double declared_dollars = static_cast<double>(shipment.declared_value_cents) / 100.0;
        const auto surcharge = static_cast<Cents>(std::ceil(declared_dollars * cents_per_declared_dollar));
        return base_cost + surcharge;
    };
}
```

Getting the same effect from expression one's class hierarchy would mean introducing a decorator class, a second pattern, just to wrap one `RateStrategy` around another. A `std::function` composes with no extra class because it is a value that closes over whatever state it needs; the Decorator pattern Part 6 covers later in this book is the idiomatic answer to the same force when the thing being wrapped is an object with more than one method, not a single callable.

## Expression three: a compile-time policy

The third expression gives up runtime swappability entirely and gets a zero-overhead quote in return. `RatePolicy` is a C++20 concept constraining a template parameter to anything exposing a `quote(const Shipment&) -> Cents` method:

```cpp
template <typename Policy>
concept RatePolicy = requires(const Policy& policy, const Shipment& shipment) {
    { policy.quote(shipment) } -> std::convertible_to<Cents>;
};

template <RatePolicy Policy>
class StaticRateCalculator {
public:
    explicit StaticRateCalculator(Policy policy) : policy_(std::move(policy)) {}
    Cents estimate(const Shipment& shipment) const { return policy_.quote(shipment); }
private:
    Policy policy_;
};
```

`FlatRatePolicy`, `WeightBasedPolicy`, and `TieredZonePolicy` each satisfy `RatePolicy` by exposing exactly that method, and `StaticRateCalculator<WeightBasedPolicy>` resolves which `quote` implementation it calls entirely at compile time. There is no vtable, no indirect call through a function pointer, and no type-erased `std::function` allocation at the call site; the compiler can see straight through `estimate()` into the concrete `quote()` body and inline it if it chooses to. The cost sits on the other side of the trade: once `shipping_strategy_demo` is built, a `StaticRateCalculator<WeightBasedPolicy>` can never become a `StaticRateCalculator<TieredZonePolicy>` without recompiling, because the policy is a template argument, not a value stored at runtime.

This is the expression to reach for on a hot path where the strategy is already known before the program starts, for instance a pricing engine that is compiled once per deployment region with that region's tariff policy baked in, and where every call-site indirection has a measurable cost. The concept is doing a specific job here too: without `RatePolicy` constraining `Policy`, a caller who passes a type missing `quote()` would get a wall of template instantiation errors from deep inside `StaticRateCalculator`; with it, the compiler reports a clear concept-check failure at the call site instead, naming exactly what the caller's type was missing.

## Typed error handling: C++20 versus C++23

All three expressions above share the same unchecked `estimate()`/`quote()` contract: pass a malformed `Shipment`, for instance one with a negative weight, and the behavior is whatever the arithmetic in `shipping_domain.cpp` happens to produce. A C++20 codebase has two conventional ways to make that failure explicit instead: return a sentinel wrapped in `std::optional`, discarding the reason for the rejection, or throw an exception, which works but forces every caller either to wrap the call in a `try` block or to accept that an unhandled exception can propagate out of a quote lookup on a checkout path. C++23's `std::expected` gives the same call site a third option: a return type that carries either the quoted price or a typed error, checkable with an `if` and no exception machinery at all. `RateEstimator::try_estimate()` in this example is guarded behind `#if __cplusplus >= 202302L` precisely so the mandatory C++20 build path never needs `std::expected`, while the C++23 build gets the more precise contract.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: std::optional loses the reason for rejection; the caller
// learns *that* a shipment was invalid, not *why*.
std::optional<Cents> try_estimate(const Shipment& shipment) const {
    if (shipment.weight_kg < 0.0) {
        return std::nullopt;
    }
    return strategy_->quote(shipment);
}
```
```cpp
// C++23: std::expected carries a typed error alongside the success
// value, checkable with an if and no exception machinery.
std::expected<Cents, std::string> try_estimate(const Shipment& shipment) const {
    if (shipment.weight_kg < 0.0) {
        return std::unexpected<std::string>("shipment weight_kg cannot be negative");
    }
    return strategy_->quote(shipment);
}
```

Both versions compile against the same `RateStrategy` and the same carriers; the only thing that changes between standards is how `RateEstimator` reports a rejected shipment to its caller. That is the pattern this book's codetabs convention exists to show: Strategy itself, the decision to vary the quoting algorithm behind a stable interface, is identical in both columns. Only the idiom available to express one small piece of its contract, the failure path, moved forward between C++20 and C++23.

> **AI sidebar.** Naming the pattern out loud, "this needs to be a Strategy because the carrier rule varies independently of checkout," gives you and an LLM pairing partner the same shared vocabulary Chapter 1 described for a human reviewer. It also sets up a specific thing to check in whatever the model hands back: an LLM trained on a large corpus of C++ tutorials reaches for expression one, the abstract base class and `unique_ptr`, by reflex, because it is the version that appears in the most textbooks. Read the forces before accepting that shape. If the strategies only ever get swapped by reassigning a value, not by polymorphic ownership across module boundaries, a `std::function` is a smaller, equally correct answer with no virtual dispatch at all; if the strategy is fixed at compile time and performance is on the line, a concept-constrained template is smaller still. A generated `RateStrategy` interface with exactly one caller and no second implementation on the roadmap is the clearest sign the model reached for the familiar shape instead of the one the forces call for.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/08-strategy` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 18 cases under the `cpp23` preset, 16 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 18   # cpp23
100% tests passed, 0 tests failed out of 16   # cpp20
```

To see the three strategies price the identical `Shipment` side by side rather than just reading an assertion that says they agree, run the example's own demo script:

```sh
examples/08-strategy/demo.sh cpp23
```

It prints each expression's quote for the same 3.5 kg shipment headed to a `Regional` destination zone with a declared value of $120:

```
== Strategy 1: runtime polymorphism (unique_ptr injection) ==
Weight-based carrier: $10.25
Tiered-zone carrier (swapped at runtime): $12.30

== Strategy 2: std::function value-semantic callable ==
Flat-rate carrier: $9.99
Tiered-zone carrier + insurance (composed): $14.10

== Strategy 3: compile-time policy (concept-constrained template) ==
Weight-based policy: $10.25
Tiered-zone policy: $12.30

== C++23 refinement: typed validation via std::expected ==
Rejected malformed shipment: shipment weight_kg cannot be negative
```

Notice that "Weight-based carrier" and "Weight-based policy" both quote $10.25 for the same shipment, and "Tiered-zone carrier" and "Tiered-zone policy" both quote $12.30. That agreement is the shared `shipping_domain.cpp` rate math showing up as an observable fact at the terminal, not just as a passing test. Running `examples/08-strategy/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## When not to use Strategy

Strategy earns its abstraction when an algorithm varies independently of the code around it, and when more than one implementation is either in hand already or clearly coming. It stops paying for itself well before that.

A single fixed algorithm with no credible second implementation does not need a `RateStrategy` interface, a `std::function`, or a policy template; it needs a function. If every shipment in a deployment used the same tiered-zone rate table and no contract on the roadmap ever changes that, wrapping `tiered_zone_cost()` in any of the three expressions in this chapter adds a layer of indirection with nothing underneath it to justify the cost, whether that cost is a virtual call, a type-erased allocation, or a template parameter a caller now has to name explicitly. The plainer non-pattern there is calling `tiered_zone_cost(shipment)` directly from checkout code, the same candidate-non-pattern move Chapter 1 trained: name the pattern and the plain alternative side by side, and let the forces, not the pattern's familiarity, decide between them.

A branch that is not expensive to extend is also a sign Strategy is premature. Three carriers, checked in sequence by a four-line `if`/`else if` chain inside a single function that one team reads start to finish, do not need an abstract interface; the chain is the whole story, visible at a glance, and a reviewer can confirm it is correct without jumping to three other files. The point where that calculus flips is the point Chapter 1 described for the billing engine's fee tiers: the carrier list grows past a handful, different carriers' pricing logic is owned by different teams who should not need to touch one shared `if` chain to add a carrier, or a carrier choice must be swapped at runtime from configuration rather than fixed at compile time. Until one of those is true, the plain branch or the plain function is the stronger engineering choice, even with Strategy's name sitting right there, fresh from this chapter.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley): the original Strategy entry, including the intent statement this chapter's three expressions all satisfy in different idioms.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/08-strategy`'s suite, which asserts that all three expressions quote an identical `Shipment` identically rather than just checking that the binary exits zero.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
