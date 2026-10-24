---
title: "CRTP and policy-based design"
order: 21
part: "idiomatic-cpp-expressions"
description: "Two idiomatic C++ expressions of static polymorphism, taught against the same shipping-rate domain Chapter 8 and Chapter 9 use: a CRTP base that resolves a derived type's method at compile time, a reusable CRTP mixin applied across unrelated types, and a policy-composed host class with zero runtime dispatch cost anywhere."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a CRTP base class template that resolves to a derived type's method through `static_cast`, with no virtual function and no vtable anywhere in the hierarchy.
- Write a reusable CRTP mixin, not tied to any one class hierarchy, and explain why the self-type trick works across completely unrelated types.
- Compose a host class from two or more independent, concept-constrained compile-time policies, and explain what changes when a caller swaps one policy versus swapping both.
- State precisely when static polymorphism beats virtual dispatch, in terms this book has already built: Chapter 8's third Strategy expression and Chapter 9's CRTP-versus-deducing-this contrast, both named the tradeoff; this chapter builds the mechanism underneath it.
- Rewrite a CRTP base with C++23's deducing `this`, and explain which costs that removes and which it does not.
- Recognize when a CRTP hierarchy or a policy-templated class is solving a problem that a plain runtime interface already solves better, and say what to write instead.

## Why this matters

Chapter 8 named a scenario it did not build: "a pricing engine that is compiled once per deployment region with that region's tariff policy baked in", a hot path where the strategy is already known before the program starts and every call-site indirection has a measurable cost. That scenario is this chapter's starting point, not a new one. A checkout library deployed as a shared object into a fleet of regional storefronts can be built once per region with that region's rounding rule and surcharge schedule fixed at compile time, because a region's tariff rarely changes and a quote on a checkout path is latency-sensitive enough that a virtual call or a type-erased `std::function` allocation is a cost worth avoiding. Chapter 8's third Strategy expression, a concept-constrained template parameter, already showed the entry point to this idea for a single varying algorithm. This chapter builds what sits underneath it: the CRTP idiom that makes a base class resolve to a derived type's method without a virtual function at all, and policy-based design, the technique for composing a class out of more than one independently swappable compile-time choice, not just one.

Chapter 9 touched CRTP too, briefly, contrasting it with C++23's deducing `this` for a single hook with no precondition to protect. That contrast is correct as far as it goes, but it is narrow by design: one hook, one derived class, a comparison meant to be read in five minutes. Here, a CRTP base carries more than one concrete derived type, the ordinary shape a reader meets in real code; a CRTP mixin has no connection to any particular class hierarchy, proving the self-type trick is a general tool rather than a narrow one-hook illustration; and policy-based design sits on top of CRTP's foundation as a host class template parameterized by more than one independent policy, each swappable without touching the others.

{% include excalidraw.html name="21-crtp-policy" caption="Figure 21.1: CRTP resolves a derived type at compile time instead of through a vtable; a policy host is composed from interchangeable compile-time policies the same way." %}

## The domain: a compile-time shipping quote

The example behind this chapter lives in `examples/21-crtp-policy`, and it reuses the `Shipment` and `Cents` conventions Chapter 8 and Chapter 9 established: weight in kilograms, a declared value in integer cents, money that is always exact. Nothing about the domain itself is new here. What changes is the mechanism every quote runs through: every type in this chapter is a class template, a concept, or a `constexpr`-friendly ordinary class, resolved entirely during compilation, with no translation unit worth compiling into a separate library the way Chapter 8's shared `shipping_domain.cpp` was.

```cpp
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    Cents declared_value_cents;
};
```

## CRTP: a base template that resolves itself at compile time

The Curiously Recurring Template Pattern gets its name from the construction every concrete type using it has to write: a class inherits from a template instantiated with itself as the argument. `RateModel<Derived>` is the base; `FlatRateModel` and `WeightedRateModel` are two concrete rate models that each name themselves as `RateModel`'s template parameter.

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

`quote()` is not virtual, and `RateModel<Derived>` declares no virtual function anywhere. The `static_cast<const Derived*>(this)` inside it is the entire mechanism: at the point `quote()` is instantiated for `RateModel<FlatRateModel>`, the compiler already knows `Derived` is `FlatRateModel`, so the cast is not a runtime check of any kind, just a reinterpretation the type system already guarantees is safe because `FlatRateModel` inherits from exactly that specialization of `RateModel`. The call to `compute()` that follows is an ordinary, fully resolved function call the compiler can inline straight through, the same way it would inline a call to any other non-virtual method.

This is what "no vtable" means concretely, not just as a slogan. `RateModel<FlatRateModel>` and `RateModel<WeightedRateModel>` are two different, unrelated types as far as the compiler is concerned, each with its own fully specialized `quote()`. Nothing stores a pointer to a shared base at runtime and nothing looks one up through an indirect table; the example's test suite proves this by writing a function template that takes a `RateModel<Derived>` by reference and calls `quote()` through it:

```cpp
template <typename Derived>
Cents quote_via_base(const RateModel<Derived>& model, const Shipment& shipment) {
    return model.quote(shipment);
}
```

`quote_via_base(flat_model, shipment)` and `quote_via_base(weighted_model, shipment)` dispatch correctly to `FlatRateModel::compute()` and `WeightedRateModel::compute()` respectively, even though both go through a reference to the same-looking base-class template. Compare this to Chapter 8's first Strategy expression, where `RateEstimator` holds a `std::unique_ptr<RateStrategy>` and `quote()` is resolved through a vtable lookup at the moment it is called, every single time. `RateModel<Derived>` never has that moment: the lookup already happened, once, when the compiler instantiated `quote()` for that particular `Derived`.

The example's header carries a `static_assert` that proves this at compile time rather than only at test time:

```cpp
static_assert(FlatRateModel(500).quote(Shipment{9.0, 0}) == 500,
              "RateModel<FlatRateModel>::quote must resolve to FlatRateModel::compute at compile time");
```

If a future edit to `RateModel` ever broke the dispatch, for instance by accidentally calling a method on `this` instead of the cast `Derived*`, this line would stop compiling. A gtest case can only tell you a binary that already built is behaving correctly; a `static_assert` tells you the behavior holds before the binary exists at all.

## A reusable CRTP mixin, not tied to one hierarchy

`RateModel<Derived>` is useful, but it is still a base class for one family of related types, rate models. CRTP's self-type trick is not limited to that shape. `Comparable<Derived>` in `src/comparable_mixin.hpp` is a mixin with no relationship to rate models whatsoever: any `Derived` that exposes a `sort_key()` method can inherit it and gain `operator<`, `operator==`, and `operator>` with no code of its own to write for any of them.

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

`ShipmentQuote` orders by an integer `Cents` key; `CarrierRating` orders by a floating-point score. Neither inherits from the other and neither is aware the other exists; both inherit `Comparable<Derived>` independently, each getting its own fully resolved `operator<` generated at the point it is instantiated for that `Derived`. `Comparable<Derived>` adds no data member of its own, so `sizeof(ShipmentQuote)` is exactly the size of its one `Cents` field; the empty base contributes nothing to the object's layout. This is the same construction the standard library itself uses in `std::enable_shared_from_this<Derived>`, which every C++ programmer who has written a class needing `shared_from_this()` has already inherited from without necessarily calling the technique by name. A reviewer who has learned to recognize `Base<Derived>` in one place recognizes it everywhere it appears afterward, including in library code they did not write.

`Derived` in a CRTP base can itself be constrained by a C++20 concept, the same tool Chapter 8 used to constrain a policy parameter. `Comparable` above does not bother, because a missing `sort_key()` produces a reasonably clear error at the point `operator<` tries to call it, but a CRTP base whose generated code is deeper or whose error would otherwise surface three template instantiations away benefits from the same `requires` clause Chapter 8's `RatePolicy` used, naming exactly what `Derived` is missing instead of leaving a reader to decode a page of compiler output.

## Policy-based design: composing more than one axis at compile time

Chapter 8's third Strategy expression constrained a single template parameter, `Policy`, to a type with a `quote()` method, and that was the whole story: one axis varies, one template parameter carries it. Policy-based design is what happens when more than one independent decision needs to vary, and each decision gets its own template parameter rather than being folded into a single monolithic policy type that would otherwise have to cover every combination by hand.

`ShippingCalculator<Rounding, Surcharge>` in `src/shipping_calculator.hpp` has two independent policy slots, each constrained by its own concept:

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

`NoRoundingPolicy` and `RoundUpToNickelPolicy` satisfy `RoundsCents`; `NoSurchargePolicy`, `FlatSurchargePolicy`, and `DeclaredValueSurchargePolicy` satisfy `SurchargesShipment`. Swapping `Rounding` from `NoRoundingPolicy` to `RoundUpToNickelPolicy` changes `quote()`'s behavior without touching `Surcharge` at all, and the reverse holds too: the two axes are independent, not two names for the same underlying choice. `ShippingCalculator<RoundUpToNickelPolicy, FlatSurchargePolicy>` and `ShippingCalculator<RoundUpToNickelPolicy, DeclaredValueSurchargePolicy>` are two different, fully resolved types, each with its own `quote()` the compiler can see straight through to both policies' bodies.

Policy-based design differs from Chapter 8's single-policy expression three in exactly this way: the host class does not grow a branch, a flag, or a second constructor to support a second varying decision. It grows a second template parameter, constrained by its own concept, and the two compose by simple template instantiation rather than by any logic `ShippingCalculator` itself has to write or test. A quote that needs a third independent axis, a currency-conversion policy, say, would add a third template parameter the same way, not a new code path inside `quote()`.

A `static_assert` again proves the simplest composition resolves entirely at compile time:

```cpp
static_assert(ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy>(1000).quote(
                  Shipment{2.0, 0}) == 1000,
              "a no-rounding, no-surcharge ShippingCalculator must quote exactly the base rate");
```

## CRTP versus deducing `this`, applied to this chapter's own base

Chapter 9 drew the CRTP-versus-deducing-`this` contrast for a single narrow hook, `summarize()`, built just to show the mechanics and set aside immediately afterward. This chapter draws the same contrast against `RateModel`, the base this chapter's CRTP section teaches, to show what the C++23 idiom changes and what it leaves untouched when applied to a base with real derived types already using it.

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

`DeducedFlatRateModel` inherits from `DeducedRateModel` the way it would inherit from any ordinary base class, naming nothing about itself in the inheritance list, because `DeducedRateModel` is not a template at all. Both versions compile to the same zero-overhead call, resolved entirely at compile time with no vtable in either column; the example's test suite asserts the two agree on the same `Shipment`. What deducing `this` removes is exactly what Chapter 9 already named: the CRTP template argument and the explicit `static_cast`, both constructions a reader has to have explained to them once before they stop looking strange. What it does not remove is the thing this chapter has been building toward: neither version pays a virtual-call cost, and neither buys back the ability to swap `Derived` after the program is compiled. Deducing `this` is a syntactic refinement of CRTP's self-type trick, not a different answer to when static polymorphism is the right tool. Chapter 9's policy pattern for applying it still holds here: reach for it on a single hook with a real, measured reason to avoid both the vtable and the CRTP boilerplate; a reviewer unfamiliar with either idiom reads a plain virtual method fastest of all three.

## When static polymorphism beats virtual dispatch

The forces are the same ones Chapter 8 trained for its three Strategy expressions, pushed one notch further: static polymorphism wins when the choice is fixed before the program runs and the call site is hot enough that an indirect call or a heap-allocated type-erased callable is a cost worth avoiding. A region-specific shipping calculator compiled once per deployment, a tight inner loop in a pricing engine invoked per line item on an order with thousands of lines, a hardware abstraction layer selected at build time because the target device never changes after the firmware image is flashed: each of these fixes its "which implementation" question once, at build time, and then calls the chosen implementation millions of times without ever needing to ask the question again. CRTP and policy-based design both answer exactly that question with zero runtime cost: no vtable lookup, no `std::function`'s type-erased allocation and indirect call, nothing standing between the call site and the concrete code that will run.

Virtual dispatch wins back the moment the choice has to be made after the program starts: a carrier selected from a configuration file read at startup, a plugin loaded from a shared library the core binary was never compiled against, a strategy a user picks from a menu while the program is already running. Chapter 8's first two Strategy expressions exist for exactly that case, and nothing in this chapter changes that answer. A `RateModel<Derived>` cannot become a different `RateModel<OtherDerived>` after it is constructed, the same way Chapter 8 noted a `StaticRateCalculator<WeightBasedPolicy>` cannot become a `StaticRateCalculator<TieredZonePolicy>` without recompiling; the type itself is the decision, fixed the moment the compiler finishes.

> **AI sidebar.** Ask an LLM pairing partner for "compile-time polymorphism" or "zero-overhead pluggable behavior" and a frequent result reaches for `std::variant<PolicyA, PolicyB>` plus `std::visit`, because that shape also avoids an abstract base class and reads as modern C++. Check what it resolved: `std::visit` dispatches on a runtime tag stored inside the `variant`, a jump table rather than a vtable, but still a runtime branch evaluated on every call, not a decision the compiler made once during instantiation. That is a legitimate answer to "the strategy varies, but I want to avoid an abstract base class," the same force Chapter 8's second expression already names, just expressed through a sum type instead of a `std::function`. It is not an answer to "the strategy is fixed before the program runs and I need zero indirection," the force this chapter's CRTP and policy-based design answer. The question worth asking before accepting either generated shape is the same one Chapter 1 trained: is the choice made once, at compile time, by whoever builds this binary, or can it still change while the program is running? A `variant`-and-`visit` answer to the first question has solved a problem the code does not have.

## When not to use CRTP or policy-based design

CRTP and policy-based design earn their complexity when the choice is fixed before compilation and a measured cost at the call site justifies paying for a template-heavy design. Outside that case, they stop paying for themselves quickly, in three specific ways.

Code bloat is the first and most literal cost. Every distinct `Derived` instantiates its own copy of `RateModel<Derived>::quote()`, and every distinct `<Rounding, Surcharge>` combination instantiates its own copy of `ShippingCalculator`. Three rate models and two rounding policies crossed with three surcharge policies is already eight separate class instantiations, each with its own compiled code, even though the logic they share is identical apart from the policy calls. A single runtime `RateStrategy` interface or a single `ShippingCalculator` taking runtime policy objects through an abstract base pays for one compiled implementation and dispatches through it; the compile-time version pays in binary size for every combination a build uses. For a handful of combinations this is a rounding error in a modern binary's size; for a combinatorial policy space with a dozen axes, it is a build-time and binary-size cost a team should measure before committing to the design, not assume away.

Compile time is the second cost, and it compounds with the first. Header-only template code, which both CRTP and policy-based design are in this chapter's example, gets recompiled in every translation unit that includes it, and a heavily templated header with several instantiations in play can measurably slow a team's build the moment it is included from more than a handful of `.cpp` files. A single fixed algorithm with no second implementation in hand or coming does not need a template parameter at all; Chapter 8 already named the plainer alternative, a function, for exactly that shape, and the same answer holds here: write `RateModel` as an ordinary class with one `compute()` body until a second implementation is needed, not in anticipation of one.

The third case is the one this chapter has repeated throughout: when the choice has to be made after the program starts, from a configuration file, a user selection, or a plugin loaded at runtime, static polymorphism cannot answer the question at all, no matter how it is written. A `RateModel<Derived>` cannot be reassigned to a different `Derived` once constructed, and a `std::vector` cannot hold a mix of concrete `RateModel<FlatRateModel>` and `RateModel<WeightedRateModel>` objects the way Chapter 8's `std::vector<std::unique_ptr<RateStrategy>>` could, because the two specializations are unrelated types with no common runtime base to store them behind. Needing a heterogeneous collection chosen at runtime is the clearest signal that Chapter 8's first or second Strategy expression is the right chapter to reach for, not this one.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/21-crtp-policy` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 13 cases under the `cpp23` preset, 11 under `cpp20`, the difference being the two deducing-`this` cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 13   # cpp23
100% tests passed, 0 tests failed out of 11   # cpp20
```

To see the CRTP and policy-based quotes side by side rather than just reading an assertion that says each one is correct, run the example's own demo script:

```sh
examples/21-crtp-policy/demo.sh cpp23
```

It quotes a 3.5 kg shipment with a declared value of $123.45 through both the CRTP rate models and three policy combinations, then through the deducing-`this` rewrite:

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

Notice the flat rate model and the deduced flat rate model both quote $9.99 for the same flat-rate construction, the CRTP and deducing-`this` versions of the identical idea producing identical output from two different C++ standards. Running `examples/21-crtp-policy/demo.sh cpp20` prints the same output minus the final deducing-`this` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley): the Strategy and Template Method entries this chapter's compile-time idioms refine into zero-overhead C++, the same lineage Chapter 8 and Chapter 9 trace from the Gang of Four's runtime-oriented originals.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/21-crtp-policy`'s suite, which asserts CRTP dispatch through a base-class reference and cross-checks the CRTP and deducing-`this` versions against each other, not just against a finished number.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
