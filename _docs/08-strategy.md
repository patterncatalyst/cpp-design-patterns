---
title: "Strategy"
order: 8
part: "selecting-varying-behavior"
description: "Three idiomatic C++ ways to select a varying algorithm at runtime or compile time, taught against a real shipping-rate example: an abstract interface behind a unique_ptr, a std::function value, and a compile-time policy constrained by a concept."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the forces that make Strategy the right candidate for a varying algorithm, and the plain function it competes against.
- Build a Strategy with runtime polymorphism: an abstract interface, concrete implementations, and a `std::unique_ptr` the owner can swap at runtime.
- Build the same Strategy as a value-semantic `std::function`, and compose two strategies without a class hierarchy.
- Build the same Strategy as a compile-time policy constrained by a concept, with no runtime dispatch.
- Choose between `std::optional`-or-throw and `std::expected`, new in C++23, for a Strategy's failure path.
- Recognize when a Strategy solves a problem nobody has yet, and say what to write instead.

## Why this matters

A checkout has to quote a shipping cost, and the rule varies by carrier contract: a flat rate, a price by weight, or a table tiered by zone and weight. The rule changes; the checkout around it doesn't. Modern C++ gives three ways to hold that varying rule, each with its own cost and guarantee, and picking one is the decision that matters.

{% include excalidraw.html name="08-strategy-forces" caption="Figure 8.1: Strategy's forces resolved three idiomatic ways: runtime polymorphism behind a unique_ptr, a value-semantic std::function, and a compile-time policy constrained by a concept." %}

## Pattern: Strategy

### The problem

A merchant prices shipments under several carrier contracts. The contract in force is chosen per carrier, sometimes from configuration at startup, sometimes when a carrier is swapped mid-session. Putting every rule into one function with a `switch` on the carrier means every new contract edits checkout code, and a bug fixed in one branch stays broken in a copied neighbor.

### The solution

A **Strategy** puts each variant of an algorithm behind one interface, so the code that uses it depends on what the algorithm does and not on which one it is.

Use a Strategy when:

- two or more implementations exist now or clearly will;
- the choice is made outside the calling code, from a contract, a configuration file or a runtime swap;
- different owners maintain different variants.

### How C++ models it

A `Shipment` carries a weight, a `DestinationZone` and a declared value. Money is an integer `Cents`, never a `double`, so quotes compare exactly:

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

The rate math lives once, in `shipping_domain.cpp`, as `flat_rate_cost`, `weight_based_cost` and `tiered_zone_cost`. All three Strategy expressions call down into it, so an identical `Shipment` prices identically whichever mechanism quotes it, and a fix to the math reaches all three.

| Expression | Swap at runtime | Cost per call | Pick it when |
| --- | --- | --- | --- |
| Abstract interface + `unique_ptr` | Yes, by replacing the owned object | Virtual call, heap allocation | The strategy is an object with state or several methods, chosen from outside the program |
| `std::function` value | Yes, by assignment | Type-erased call | The strategy is one callable, and you want to compose two into a third |
| Concept-constrained template | No; fixed at compile time | None; the call can inline | The choice is known before the program runs and the path is hot |

### Runtime polymorphism behind a unique_ptr

The GoF shape is an abstract interface and a family of implementations. `RateStrategy` declares one pure virtual `quote`, and `FlatRateCarrier`, `WeightBasedCarrier` and `TieredZoneCarrier` implement it:

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

`RateEstimator` owns one carrier at a time through a `std::unique_ptr`, injected through the constructor and replaceable with `use_strategy()`:

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

The `unique_ptr` states ownership in the type: nothing else shares the carrier, and when `use_strategy()` replaces it, the old carrier's destructor runs with no `delete` anywhere. This is RAII applied to polymorphism. It's also the expression to use when a carrier name read from configuration maps to a concrete class through a small factory, since any concrete carrier converts to `unique_ptr<RateStrategy>`.

### A std::function value

The second expression keeps runtime swapping without a base class. A strategy is any callable with the right signature:

```cpp
using RateFn = std::function<Cents(const Shipment&)>;

RateFn weight_based(Cents base_cents, Cents per_kg_cents) {
    return [base_cents, per_kg_cents](const Shipment& shipment) {
        return weight_based_cost(base_cents, per_kg_cents, shipment.weight_kg);
    };
}
```

`QuoteBook` holds one `RateFn` by value and reassigns it with `use()`. There's no heap-owned polymorphic object and no virtual destructor to get right; the strategy copies and moves like any value.

The value form composes. `with_insurance_surcharge()` wraps an existing `RateFn` and adds a surcharge proportional to the declared value:

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

The class-hierarchy version needs a decorator class to wrap one `RateStrategy` in another. A closure does it in one function. When the wrapped thing has more than one method, the Decorator pattern is the right tool.

### A compile-time policy

The third expression gives up runtime swapping for a call with no indirection. `RatePolicy` is a concept that accepts any type with a `quote(const Shipment&)` returning something convertible to `Cents`:

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

`FlatRatePolicy`, `WeightBasedPolicy` and `TieredZonePolicy` satisfy it. `StaticRateCalculator<WeightBasedPolicy>` resolves its `quote` at compile time: no vtable, no function pointer, no type erasure, and the compiler can inline the body into `estimate()`. The price is that a `StaticRateCalculator<WeightBasedPolicy>` never becomes a `StaticRateCalculator<TieredZonePolicy>` without recompiling.

Use it on a hot path where the policy is known at build time, such as a pricing engine built once per region with that region's tariff baked in. The concept also improves the error. Instantiating `StaticRateCalculator` with a type that has no `quote` fails at the call site with `constraints not satisfied for class template 'StaticRateCalculator'`, followed by a note naming the missing member, instead of an error from deep inside the template body.

### The failure path: optional, throw or expected

All three expressions share an unchecked contract: a `Shipment` with a negative weight gets whatever the arithmetic produces. A C++20 caller has two ways to make the failure explicit:

- return `std::optional`, which says *that* the shipment was rejected but not *why*;
- throw, which forces every checkout caller to catch or let the exception escape a quote lookup.

`std::expected`, new in C++23, returns either the quote or a typed error, checked with an `if`. `RateEstimator::try_estimate()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never needs `<expected>`:

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

The Strategy is the same in both columns. Only the way `RateEstimator` reports a rejected shipment changes.

## When not to use it

- **One fixed algorithm.** If every shipment uses the same tiered-zone table and no contract will change it, call `tiered_zone_cost(shipment)` from checkout. Any of the three expressions adds a virtual call, a type-erased call or a template parameter with nothing behind it.
- **A short branch one team owns.** Three carriers in a four-line `if`/`else if` inside one function are visible at a glance and easy to review. Move to a Strategy when the carrier list grows past a handful, when different teams own different carriers' pricing, or when the choice must come from configuration at runtime.
- **A compile-time policy where the choice is a runtime input.** If the carrier is read from a request, a template parameter can't hold it; use the `std::function` or the interface.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a strategy.** "Make the shipping rate pluggable" leaves open which of the three expressions to use, where the rate math lives and how a bad shipment is reported. State them:

> In `examples/08-strategy`, add a dimensional-weight carrier. Put the rate math in one free function in `shipping_domain.cpp`, then expose it three ways that all call that function: a `RateStrategy` subclass for `RateEstimator`, a `RateFn` factory for `QuoteBook`, and a policy type that satisfies the `RatePolicy` concept for `StaticRateCalculator`. Keep money in `Cents`, round fractional cents up, and add tests that quote the same `Shipment` through all three and expect the same `Cents`.

Run `ctest --preset cpp23` and check that the reply added no second copy of the rate math.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/08-strategy`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 18 cases pass under `cpp23` and 16 under `cpp20`; the two `std::expected` cases compile only under C++23. To see the three expressions price the same shipment:

```sh
examples/08-strategy/demo.sh cpp23
```

It quotes a 3.5 kg `Regional` shipment declared at $120:

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

The weight-based carrier and policy both quote $10.25, and the tiered-zone pair both quote $12.30, because they share one rate function. Under `cpp20` the output stops before the `std::expected` block.

- **Truncate instead of rounding up.** In a scratch copy, drop the `std::ceil` in `weight_based_cost`. One case fails under `cpp23`: a fractional cent now rounds down.
- **Pass a policy with no `quote`.** Instantiate `StaticRateCalculator` with an empty struct. The build fails with `constraints not satisfied for class template 'StaticRateCalculator'` and a note that the type has no member named `quote`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Strategy entry, whose intent all three expressions satisfy.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/08-strategy`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
