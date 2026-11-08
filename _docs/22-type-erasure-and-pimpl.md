---
title: "Type erasure and PImpl"
order: 22
part: "idiomatic-cpp-expressions"
description: "Type erasure as a value-semantic wrapper around a hand-written concept/model pair that holds unrelated quoters with no shared base class, and PImpl as a compilation firewall: correct special members, ABI stability, and when each idiom earns its cost."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Hold unrelated concrete types behind one value type, with no shared base class.
- Build a concept/model pair behind a value-semantic wrapper, the technique `std::function` and `std::any` use.
- Choose between `std::function`, a Decorator and a hand-written erased type.
- Explain why a PImpl type's one-pointer layout keeps a library's ABI stable.
- Write the special members a PImpl type needs, and say which ones an incomplete `Impl` forces.
- Tell when a plain interface or a plain class is enough.

## Why this matters

A checkout session asks several unrelated quoters for a price in one request: a merchant's flat-rate table, a weight-based calculation from a rating service, a surge quoter a promotions team added for one sale. None was written against a shared interface, and some aren't yours to change. Checkout only calls one method and reads back a price.

The ledger those quotes go into has the opposite problem. A header that exposes how the ledger stores entries makes every caller recompile when that storage changes, and in a shipped library it breaks binaries built against the old layout. Type erasure answers the first problem with a concept/model pair behind a value. PImpl answers the second with a pointer to an incomplete type behind a stable header.

{% include excalidraw.html name="22-type-erasure-pimpl" caption="Figure 22.1: AnyRateQuoter erases unrelated concrete quoters behind a hand-written concept/model pair with no shared base class; RateQuoteLedger exposes only its header across a PImpl compilation firewall, with the hidden Impl reachable through nothing but a pointer." %}

The domain is the shipment from Chapter 8:

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

Four quoters share nothing but a method shape:

| Quoter | Holds |
| --- | --- |
| `FlatRateQuoter` | A flat rate |
| `WeightBasedQuoter` | Base and per-kilogram rates |
| `SurgeQuoter` | A mutable counter that raises the price on each repeat call |
| `InsuredQuoter` | Another quoter, plus a surcharge on the declared value |

None inherits from anything, and `RateQuoteLedger` records their quotes without knowing their types.

## Pattern: Type erasure

### The problem

Checkout needs a `std::vector` of quoters of unrelated types. A shared abstract base would mean editing every quoter, including the ones another team or a third-party library owns, and a template parameter would make each container element a different type.

### The solution

**Type erasure** wraps any type that supports a required operation in one fixed value type, by hiding the concrete type behind a private polymorphic interface the wrapper owns.

### How C++ models it

The only agreement is a concept, not a base class:

```cpp
template <typename Quoter>
concept Quotable = requires(const Quoter& quoter, const Shipment& shipment) {
    { quoter.quote(shipment) } -> std::convertible_to<Cents>;
};
```

`AnyRateQuoter` has no template parameter, so values built from different quoters share one type. `Concept` is the erased interface, nested privately, and `Model<Quoter>` adapts one concrete quoter to it by holding it and forwarding calls:

```cpp
class AnyRateQuoter {
public:
    template <Quotable Quoter>
    AnyRateQuoter(Quoter quoter) : model_(std::make_unique<Model<Quoter>>(std::move(quoter))) {}

    Cents quote(const Shipment& shipment) const { return model_->quote(shipment); }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual Cents quote(const Shipment& shipment) const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    template <Quotable Quoter>
    struct Model : Concept {
        explicit Model(Quoter quoter) : quoter_(std::move(quoter)) {}
        Cents quote(const Shipment& shipment) const override { return quoter_.quote(shipment); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(quoter_); }
        Quoter quoter_;
    };

    std::unique_ptr<Concept> model_;
};
```

`std::function` and `std::any` work the same way inside. The difference is what the wrapped type must provide:

| Varying thing | Use |
| --- | --- |
| Any callable with one signature | `std::function<Cents(const Shipment&)>`, as in Chapter 8 |
| Any type with a named operation, or several | A hand-written concept/model pair |

### Copies are deep

`std::unique_ptr` is move-only, so the compiler doesn't generate a copy constructor for `AnyRateQuoter`. The hand-written one calls `Concept::clone()`, and each `Model` copies its own `quoter_`. The destructor and move members stay defaulted, because `Concept` and `Model` are fully defined in the header; nothing is incomplete.

A copied `SurgeQuoter` keeps its own counter. Quote the original once, copy the wrapper and quote the copy twice: the original's next quote is still its second, $10.25, not its fourth.

### Stacking without inheritance

`InsuredQuoter` holds an `AnyRateQuoter` as a data member and exposes its own `quote()`. That satisfies `Quotable`, so an `InsuredQuoter` can be erased into another `AnyRateQuoter` and stacked again. Three idioms add one behavior onto another without a class per combination:

| Idiom | How it wraps |
| --- | --- |
| Chapter 8's `with_insurance_surcharge()` | A `std::function` closing over a captured base rate |
| Chapter 16's Decorator | Inherits the interface it wraps |
| `InsuredQuoter` | Holds an erased value; no interface declared |

## Pattern: PImpl

### The problem

`RateQuoteLedger` will grow: a running average, a per-carrier breakdown, a timestamp per entry. If those members sit in the header, every caller recompiles on each change, and binaries linked against an older build of the library compute the wrong offsets and `sizeof`, with no diagnostic.

### The solution

**PImpl** (pointer to implementation) moves a class's data and private helpers into an `Impl` type defined only in the `.cpp` file, so the header holds one pointer and its layout never changes.

### How C++ models it

`RateQuoteLedger` holds a single `std::unique_ptr<Impl>`, the shape Chapter 15 built for `LabelRenderer`, and `sizeof(RateQuoteLedger)` equals `sizeof(void*)`. Its special members:

| Member | Why |
| --- | --- |
| Destructor, out of line | `std::default_delete<Impl>` needs `Impl`'s full definition, which exists only in `rate_quote_ledger.cpp` |
| Copy constructor and copy assignment | Deep-clone `Impl` rather than copy the pointer |
| Move constructor and move assignment | Defaulted in the `.cpp`; moving a pointer never needs the pointee |

Adding fields to `Impl` changes neither `sizeof(RateQuoteLedger)` nor its public members. That layout is the ABI that already-compiled code depends on, and it's why widely shipped C++ libraries use PImpl heavily.

### `const` doesn't propagate

`std::unique_ptr` doesn't pass constness through to its pointee. Inside `summary() const`, the pointer can't be reseated, but a non-`const` method on `*impl_` still compiles. Keep every `Impl` method that a `const` outer method calls `const` itself; the type system won't enforce it.

### Where each idiom fits

`AnyRateQuoter` can't go behind a PImpl: its constructor is a template, and a template's definition must be visible wherever it's instantiated. PImpl works on `RateQuoteLedger` because it's an ordinary class with a fixed set of operations. The open-ended part of the domain uses type erasure, and the closed, stable part uses PImpl.

### A typed rejection

`record()` throws `std::invalid_argument` on a negative amount, which forces every caller into a `try` block or an unhandled exception on the checkout path. `std::expected`, new in C++23, returns the rejection as a value. `try_record()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never needs it.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on a negative amount works, but it forces every caller
// to wrap the call in a try block or accept an unhandled exception.
void RateQuoteLedger::record(std::string label, Cents amount_cents) {
    if (amount_cents < 0) {
        throw std::invalid_argument("quote amount_cents cannot be negative");
    }
    impl_->record(std::move(label), amount_cents);
}
```
```cpp
// C++23: std::expected reports the same rejection as a typed error,
// checkable with an if and no exception machinery.
std::expected<void, std::string> RateQuoteLedger::try_record(std::string label, Cents amount_cents) {
    if (amount_cents < 0) {
        return std::unexpected<std::string>("quote amount_cents cannot be negative");
    }
    impl_->record(std::move(label), amount_cents);
    return {};
}
```

Both call the same `Impl::record()`; only how the caller learns of the rejection changes.

> **🤖 Explore with your agent**
>
> "In `examples/22-type-erasure-pimpl/src/any_rate_quoter.hpp`, write `AnyRateQuoter` as a value type over any type satisfying the `Quotable` concept, with a private `Concept` and a `Model<Quoter>`, a copy constructor that deep-copies through `clone()`, and defaulted moves. Don't change `FlatRateQuoter`, `WeightBasedQuoter`, `SurgeQuoter` or `InsuredQuoter`. Keep `RateQuoteLedger` behind a `std::unique_ptr<Impl>`, with the destructor, copies and moves declared in `rate_quote_ledger.hpp` and defined in `rate_quote_ledger.cpp`."
>
> Check: `ctest --preset cpp23 -R 'AnyRateQuoterTest|RateQuoteLedgerTest'` passes, and `rate_quoters.hpp` is unchanged.

## When not to use it

- **Type erasure.** When you own every quoter and can add a `RateStrategy` base cheaply, Chapter 8's interface and `unique_ptr` do the job with less machinery. When the varying thing is one call signature, use `std::function`.
- **PImpl.** A class with one `int` member and no churn gains nothing but five hand-written special members. Use PImpl when the implementation is large, volatile or drags in a dependency callers shouldn't include, the bar Chapter 15 set.
- **PImpl on templates.** A class whose public operations are templates can't use PImpl at all; the bodies must stay visible at every instantiation.

## Build, run, observe

From the repository root, the superbuild picks up `examples/22-type-erasure-pimpl`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 12 cases pass under `cpp23` and 10 under `cpp20`; the two `std::expected` cases compile only under C++23. The demo puts three unrelated quoters in one `std::vector<AnyRateQuoter>`, copies a surge quoter and records every quote in a ledger:

```sh
examples/22-type-erasure-pimpl/demo.sh cpp23
```

```
== Type erasure: unrelated concrete quoters in one container ==
Flat-rate quoter: $9.99
Weight-based quoter: $10.25
Insured weight-based quoter (wraps another AnyRateQuoter): $12.05

== Type erasure: a copy is independent of the original's mutable state ==
Surge quoter, call 1 (original): $10.00
Surge quoter, call 2 (copy): $10.25
Surge quoter, call 3 (copy again): $10.50
Surge quoter, call 2 (original, untouched by the copy): $10.25

== PImpl: RateQuoteLedger copies deeply, not just the pointer ==
3 quote(s), total 3229 cents:
  flat-rate quoter: 999 cents
  weight-based quoter: 1025 cents
  insured weight-based quoter: 1205 cents
original ledger entry count after copying: 3
copy's ledger entry count after one more record(): 4

== C++23 refinement: typed rejection via std::expected ==
Rejected malformed quote: quote amount_cents cannot be negative
```

The original ledger keeps 3 entries after the copy records a fourth. Under `cpp20` the last block is absent.

One experiment shows why the destructor lives in the `.cpp`:

- **Inline the destructor.** In a scratch copy, declare `~RateQuoteLedger() = default;` in the header and delete its definition from `rate_quote_ledger.cpp`. Every file that includes the header and destroys a ledger fails to compile with "invalid application of 'sizeof' to an incomplete type 'shipping::RateQuoteLedger::Impl'".

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): Bridge, which decouples an abstraction from its implementation, is the ancestor PImpl specializes for one concrete class. Type erasure isn't a GoF pattern; it's a C++ idiom drawn from how the standard library implements `std::function` and `std::any`.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/22-type-erasure-pimpl`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
