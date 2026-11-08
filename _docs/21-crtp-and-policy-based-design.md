---
title: "CRTP and policy-based design"
order: 21
part: "idiomatic-cpp-expressions"
description: "Two idiomatic C++ expressions of static polymorphism on the shipping-rate domain: a CRTP base that resolves a derived type's method at compile time, a reusable CRTP mixin across unrelated types, and a host class composed from compile-time policies with no runtime dispatch."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a CRTP base class template that reaches a derived type's method through `static_cast`, with no virtual function and no vtable.
- Write a CRTP mixin that adds operators to unrelated types.
- Compose a host class from independent, concept-constrained compile-time policies.
- Rewrite a CRTP base with C++23's deducing `this`, and say which costs that removes.
- Tell when static polymorphism beats virtual dispatch, and when a runtime interface is the better answer.

## Why this matters

A checkout library deployed into a fleet of regional storefronts can be built once per region, with that region's rounding rule and surcharge schedule fixed at compile time. A region's tariff rarely changes, and a quote on the checkout path is hot enough that a virtual call or a `std::function` allocation is a cost worth avoiding. [Strategy]({{ '/docs/08-strategy/' | relative_url }}) already showed one varying algorithm as a concept-constrained template parameter. CRTP makes a base class reach a derived type's method without a virtual function, and policy-based design composes a class from more than one independent compile-time choice.

{% include excalidraw.html name="21-crtp-policy" caption="Figure 21.1: CRTP resolves a derived type at compile time instead of through a vtable; a policy host is composed from interchangeable compile-time policies the same way." %}

The domain is the shipment from Chapter 8: weight in kilograms and a declared value in integer cents, so money stays exact. Every type here is a class template, a concept or a `constexpr` class, resolved during compilation.

```cpp
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    Cents declared_value_cents;
};
```

## Pattern: CRTP

### The problem

A family of rate models shares a public `quote()` and differs only in how each computes a rate. A virtual `compute()` would give every quote an indirect call through a vtable, and the region's build already knows which model it uses.

### The solution

The **Curiously Recurring Template Pattern** has a class inherit from a base template instantiated with that class itself, so the base can call the derived type's methods with no virtual dispatch.

### How C++ models it

`RateModel<Derived>` is the base; `FlatRateModel` and `WeightedRateModel` each name themselves as its template argument.

```cpp
template <typename Derived>
class RateModel {
public:
    constexpr Cents quote(const Shipment& shipment) const {
        return static_cast<const Derived*>(this)->compute(shipment);
    }
};

class FlatRateModel : public RateModel<FlatRateModel> {
public:
    constexpr explicit FlatRateModel(Cents flat_cents) : flat_cents_(flat_cents) {}
    constexpr Cents compute(const Shipment&) const { return flat_cents_; }
private:
    Cents flat_cents_;
};
```

`quote()` isn't virtual, and the base declares no virtual function. When the compiler instantiates `quote()` for `RateModel<FlatRateModel>`, it already knows `Derived` is `FlatRateModel`, so the `static_cast` is no runtime check, and `compute()` is an ordinary call it can inline. `RateModel<FlatRateModel>` and `RateModel<WeightedRateModel>` are unrelated types, each with its own `quote()`. A function template taking the base by reference still reaches the right `compute()`:

```cpp
template <typename Derived>
Cents quote_via_base(const RateModel<Derived>& model, const Shipment& shipment) {
    return model.quote(shipment);
}
```

Chapter 8's first Strategy expression looks up `quote()` through a vtable on every call. `RateModel<Derived>` did that lookup once, at instantiation. A `static_assert` in the header evaluates the whole call during translation:

```cpp
static_assert(FlatRateModel(500).quote(Shipment{9.0, 0}) == 500,
              "RateModel<FlatRateModel>::quote must resolve to FlatRateModel::compute at compile time");
```

A test checks a binary that already built; a `static_assert` fails the build itself.

### A mixin across unrelated types

The self-type trick isn't tied to one hierarchy. `Comparable<Derived>` gives any type with a `sort_key()` method `operator<`, `operator==` and `operator>`:

```cpp
template <typename Derived>
class Comparable {
public:
    friend constexpr bool operator<(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() < rhs.sort_key();
    }
    friend constexpr bool operator==(const Derived& lhs, const Derived& rhs) {
        return lhs.sort_key() == rhs.sort_key();
    }
    friend constexpr bool operator>(const Derived& lhs, const Derived& rhs) { return rhs < lhs; }
};
```

`ShipmentQuote` orders by an integer `Cents` key and `CarrierRating` by a floating-point score. Neither knows the other exists; each inherits `Comparable` and gets its own operators. The standard library uses the same construction in `std::enable_shared_from_this<Derived>`.

`Comparable` leaves `Derived` unconstrained, because a missing `sort_key()` fails at the call inside `operator<` with a readable error. When the generated code sits several instantiations deep, a `requires` clause on `Derived`, like Chapter 8's `RatePolicy`, names what is missing.

### Deducing `this`

C++23's explicit object parameter removes the template argument and the cast. Both versions compile to the same call, resolved at compile time, and quote the same shipment identically.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: CRTP. RateModel is a template parameterized on Derived, and
// quote() casts *this down to Derived explicitly. Every concrete rate
// model has to name itself as RateModel's template argument.
template <typename Derived>
class RateModel {
public:
    constexpr Cents quote(const Shipment& shipment) const {
        return static_cast<const Derived*>(this)->compute(shipment);
    }
};

class FlatRateModel : public RateModel<FlatRateModel> {
public:
    constexpr explicit FlatRateModel(Cents flat_cents) : flat_cents_(flat_cents) {}
    constexpr Cents compute(const Shipment&) const { return flat_cents_; }
private:
    Cents flat_cents_;
};
```
```cpp
// C++23: deducing this. DeducedRateModel is an ordinary, non-template
// class; quote() takes an explicit object parameter, and the compiler
// deduces Self as the most-derived type at the call site. No template
// argument in the inheritance list, and no static_cast anywhere.
class DeducedRateModel {
public:
    template <typename Self>
    constexpr Cents quote(this const Self& self, const Shipment& shipment) {
        return self.compute(shipment);
    }
};

class DeducedFlatRateModel : public DeducedRateModel {
public:
    constexpr explicit DeducedFlatRateModel(Cents flat_cents) : flat_cents_(flat_cents) {}
    constexpr Cents compute(const Shipment&) const { return flat_cents_; }
private:
    Cents flat_cents_;
};
```

| | CRTP | Deducing `this` |
| --- | --- | --- |
| Base is a template | Yes | No |
| Derived names itself in the base list | Yes | No |
| `static_cast` in the base | Yes | No |
| Virtual call | None | None |
| Swap the derived type after compiling | No | No |

Deducing `this` is a syntactic refinement of CRTP, not a different answer to when static polymorphism fits. A reviewer who knows neither idiom still reads a plain virtual method fastest.

## Pattern: Policy-based design

### The problem

A quote varies on two independent axes: how cents are rounded and what surcharge applies. One monolithic policy type would have to cover every combination by hand, and a flag inside the calculator would add a runtime branch.

### The solution

**Policy-based design** builds a host class template from several independent policy parameters, each constrained by its own concept and swappable without touching the others.

### How C++ models it

`ShippingCalculator<Rounding, Surcharge>` has two policy slots:

```cpp
template <typename Policy>
concept RoundsCents = requires(const Policy& policy, Cents cents) {
    { policy.round(cents) } -> std::convertible_to<Cents>;
};

template <typename Policy>
concept SurchargesShipment = requires(const Policy& policy, const Shipment& shipment) {
    { policy.surcharge(shipment) } -> std::convertible_to<Cents>;
};

template <RoundsCents Rounding, SurchargesShipment Surcharge>
class ShippingCalculator {
public:
    constexpr explicit ShippingCalculator(Cents base_cents, Rounding rounding_policy = Rounding{},
                                           Surcharge surcharge_policy = Surcharge{})
        : base_cents_(base_cents),
          rounding_policy_(std::move(rounding_policy)),
          surcharge_policy_(std::move(surcharge_policy)) {}

    constexpr Cents quote(const Shipment& shipment) const {
        const Cents raw_cents = base_cents_ + surcharge_policy_.surcharge(shipment);
        return rounding_policy_.round(raw_cents);
    }
private:
    Cents base_cents_;
    Rounding rounding_policy_;
    Surcharge surcharge_policy_;
};
```

| Concept | Policies |
| --- | --- |
| `RoundsCents` | `NoRoundingPolicy`, `RoundUpToNickelPolicy` |
| `SurchargesShipment` | `NoSurchargePolicy`, `FlatSurchargePolicy`, `DeclaredValueSurchargePolicy` |

Swapping `Rounding` changes the quote without touching `Surcharge`, and the reverse holds. Each combination is its own type, and the compiler sees through `quote()` into both policy bodies. A second varying decision adds a template parameter, not a branch or a second constructor; a currency-conversion policy would be a third parameter. A `static_assert` checks the simplest composition at compile time:

```cpp
static_assert(ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy>(1000).quote(
                  Shipment{2.0, 0}) == 1000,
              "a no-rounding, no-surcharge ShippingCalculator must quote exactly the base rate");
```

### Static or virtual dispatch

Static polymorphism wins when the choice is fixed before the program runs and the call site is hot. Virtual dispatch wins once the choice is made after startup.

| The choice is made | Use |
| --- | --- |
| At build time: a region's tariff, a per-line-item pricing loop, a hardware layer fixed when firmware is flashed | CRTP or policy-based design |
| At startup or later: a carrier from a config file, a plugin from a shared library, a user's menu pick | A virtual interface or `std::function`, as in Chapter 8 |

A `RateModel<Derived>` can't become a `RateModel<OtherDerived>` after construction. The type is the decision.

<blockquote class="ai-sidebar" markdown="1">
**Specifying compile-time polymorphism.** "Make the rate models pluggable with zero overhead" doesn't say whether the choice is fixed at build time or made at run time, and `std::variant` plus `std::visit` answers the second with a runtime tag on every call.

> In `examples/21-crtp-policy/src/shipping_calculator.hpp`, write `ShippingCalculator<Rounding, Surcharge>` as a class template with two policy parameters constrained by the concepts `RoundsCents` and `SurchargesShipment`, and policies `NoRoundingPolicy`, `RoundUpToNickelPolicy`, `NoSurchargePolicy`, `FlatSurchargePolicy` and `DeclaredValueSurchargePolicy`. Use no `std::variant`, `std::function` or virtual function. Keep `RateModel<Derived>` in `rate_model.hpp` as a CRTP base whose `quote()` calls `Derived::compute()` through `static_cast`, and add a `static_assert` that evaluates one quote of each at compile time.

Check: `ctest --preset cpp23 -R 'RateModelTest|ShippingCalculatorTest'` passes, and the reply contains no `std::visit`.
</blockquote>

## When not to use it

- **Code bloat.** Every distinct `Derived` instantiates its own `quote()`, and every `<Rounding, Surcharge>` pair its own `ShippingCalculator`. A runtime interface compiles one implementation. For a few combinations the size is negligible; for a dozen policy axes, measure binary size before committing.
- **Compile time.** Header-only templates recompile in every translation unit that includes them. A single fixed algorithm with no second implementation needs no template parameter: write an ordinary class until a second implementation arrives.
- **Runtime choice.** When a config file, a user or a plugin picks the implementation, static polymorphism can't answer. A `std::vector` can't hold `RateModel<FlatRateModel>` and `RateModel<WeightedRateModel>` together, because they share no runtime base. Use Chapter 8's `std::vector<std::unique_ptr<RateStrategy>>`.

## Build, run, observe

From the repository root, the superbuild picks up `examples/21-crtp-policy`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 13 cases pass under `cpp23` and 11 under `cpp20`; the two deducing-`this` cases compile only under C++23. The demo quotes a 3.5 kg shipment with a declared value of $123.45:

```sh
examples/21-crtp-policy/demo.sh cpp23
```

```
== CRTP: RateModel<Derived> (static polymorphism) ==
Flat rate model: $9.99
Weighted rate model: $10.25

== CRTP mixin: Comparable<Derived> across unrelated types ==
Flat quote < weighted quote? true
Carrier A rated higher than carrier B? true

== Policy-based design: ShippingCalculator<Rounding, Surcharge> ==
Base rate, no rounding, no surcharge: $10.00
Base rate + flat surcharge, rounded to the nickel: $11.55
Base rate + 1% declared-value surcharge, rounded to the nickel: $11.25

== C++23 refinement: deducing this instead of CRTP ==
Deduced flat rate model (no template argument on the base): $9.99
```

The CRTP and deducing-`this` flat models both quote $9.99. Under `cpp20` the last block is absent.

One experiment shows the compile-time check at work:

- **Change a rate.** In a scratch copy, make `FlatRateModel::compute()` return `flat_cents_ + 1`. The build stops at the `static_assert` in `rate_model.hpp`, before any test runs.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Strategy and Template Method entries that CRTP and policy-based design refine into zero-overhead C++.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/21-crtp-policy`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
