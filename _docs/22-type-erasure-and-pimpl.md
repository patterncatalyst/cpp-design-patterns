---
title: "Type erasure and PImpl"
order: 22
part: "idiomatic-cpp-expressions"
description: "Type erasure as a value-semantic wrapper around a hand-written concept/model pair, taught against a rate-quoting ledger that holds unrelated concrete quoters with no shared base class, paired with a deeper look at PImpl as a true compilation firewall: correct special members, ABI stability, and when the idiom earns its cost."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the structural problem type erasure solves: holding an unbounded
  family of unrelated concrete types behind one fixed-size value, with no
  shared base class anywhere.
- Build a hand-written concept/model pair behind a value-semantic wrapper,
  the same technique `std::function` and `std::any` use internally.
- Relate type erasure to Chapter 8's `std::function` and Chapter 16's
  wrapping patterns, and say what each buys that the others do not.
- Push PImpl past Chapter 15's mechanics into why a compilation firewall
  matters for ABI stability and large-scale recompile avoidance, not just
  saved keystrokes.
- Write the special members a PImpl type needs by hand, and explain which
  ones an incomplete `Impl` forces and which one a different cause forces.
- Recognize when type erasure or PImpl is paying for machinery a plain
  template, a plain class, or a plain header would already solve on its own.

## Why this matters

A checkout session asks several unrelated things for a shipping quote
within the same request: a flat-rate table a merchant configured once, a
weight-based calculation pulled in from a rating service, maybe a one-off
surge quoter a promotions team bolted on for a single sale event. None of
these quoters were written against a shared interface, and none of them
need to be, because the only thing checkout code does with any of
them is call one method and read back a price. Chapter 8 already named
this force: an algorithm that varies independently of the code that uses
it. What is different here is that the varying thing arrives as a type,
not as a value plugged into one interface somebody designed in advance,
and the code collecting quotes from all of them has no say over what any
of those types look like beyond the one method shape it depends on.

Once a quote exists, something else happens to it that is a structural
problem of a completely different shape: the running total and the audit
trail those quotes accumulate into has to stay out of sight of everything
that merely records one. A header that exposes how a ledger stores its
entries is a header every caller recompiles against the moment that
storage detail changes, and on a codebase large enough for that cost to
matter, it stops being a cosmetic preference. Type erasure and PImpl are
this book's answer to those two different problems, and modern C++ offers
a precise, hand-written technique for each: a concept/model pair behind a
value for the first, and a pointer to an incomplete type behind a stable
header for the second.

{% include excalidraw.html name="22-type-erasure-pimpl" caption="Figure 22.1: AnyRateQuoter erases unrelated concrete quoters behind a hand-written concept/model pair with no shared base class; RateQuoteLedger exposes only its header across a PImpl compilation firewall, with the hidden Impl reachable through nothing but a pointer." %}

## The domain: a quoting ledger with no shared vocabulary

The example behind this chapter lives in `examples/22-type-erasure-pimpl`,
and it reuses the same `Shipment` this book has carried since Chapter 8: a
weight, a destination zone, and a declared value.

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

Four quoters sit on top of that domain, and they share nothing with each
other but one method shape. `FlatRateQuoter` and `WeightBasedQuoter` are
plain structs holding their own rate parameters. `SurgeQuoter` carries
mutable state, a counter that nudges the price up on every repeat call
within the same session, there specifically to prove that copying one of
these quoters produces an independent object rather than a second handle
onto the same state. `InsuredQuoter` wraps an existing quoter and adds a
surcharge proportional to the shipment's declared value. None of the four
inherits from anything, none of them know the other three exist, and
`RateQuoteLedger` records whatever any of them quote without knowing
their types either.

## Type erasure: `AnyRateQuoter` holds whatever satisfies `Quotable`

`Quotable` is the only thing any of the four quoters have to agree on, and
it is a C++20 concept, not a base class:

```cpp
template <typename Quoter>
concept Quotable = requires(const Quoter& quoter, const Shipment& shipment) {
    { quoter.quote(shipment) } -> std::convertible_to<Cents>;
};
```

`AnyRateQuoter` is the value-semantic wrapper that can hold any type
satisfying `Quotable`. It carries no template parameter itself; two
`AnyRateQuoter` values built from different concrete quoters have the
exact same type and can sit side by side in one `std::vector`. The
technique that makes this possible is a hand-written concept/model pair,
nested privately inside `AnyRateQuoter`: `Concept` is the erased
interface, and `Model<Quoter>` is a template that adapts one concrete
`Quoter` to it by holding it as a data member and forwarding every call.

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

This is the exact technique `std::function` and `std::any` use internally.
`std::function<Cents(const Shipment&)>`, the callable Chapter 8 built its
second Strategy expression from, erases every callable with a matching
call signature behind one value; `AnyRateQuoter` erases every type with a
matching named method instead. The requirement a type has to satisfy
differs, call-operator shape versus named-member shape, but the mechanism
underneath is the same: a private, polymorphic `Concept`, a template
`Model` that adapts one concrete type to it, and a `unique_ptr<Concept>`
as the only data member a caller ever sees. Reaching for a `std::function`
when the varying thing is "any callable with this signature," and
writing a dedicated concept/model pair when it is "any type with this
named operation," is the dividing line between the two: `std::function`
already is the type-erased wrapper you want for the narrower case, and
writing `AnyRateQuoter` by hand pays off only when the shape you need to
erase is not a single call operator.

`InsuredQuoter` is where this chapter's type erasure and Chapter 16's
Decorator meet without ever becoming the same technique. Chapter 16 builds
a Decorator by having a wrapper inherit the exact interface of the thing
it wraps, so a caller holding that interface cannot tell how many layers
sit underneath it. `InsuredQuoter` gets the identical result, one quoter
stacked on top of another, adding a surcharge without touching the
wrapped quoter's own pricing logic, with no interface declared anywhere:
it holds an `AnyRateQuoter` as a plain data member, exposes its own
`quote()` method, and because that method satisfies `Quotable`,
`InsuredQuoter` itself can be erased into a second `AnyRateQuoter` and
stacked again. Chapter 8's `with_insurance_surcharge()` reached the same
composition with a `std::function` closing over a captured base rate.
Three different-looking pieces of code, a `std::function` composition, a
Decorator built on inheritance, and an `AnyRateQuoter` wrapping another
`AnyRateQuoter`, are three idioms answering the same force: stack one more
behavior onto an existing one without a class per combination.

The test worth running against any type-erased wrapper is whether a copy
is a deep copy. `SurgeQuoter`'s mutable counter exists for exactly
this check: build an `AnyRateQuoter` from one, call `quote()` once to
advance its counter, copy the wrapper, then call `quote()` on the copy
three more times. If the original's own next call still reflects only the
one call it personally made, the copy is independent; if it jumps ahead by
the copy's extra calls, `AnyRateQuoter`'s copy constructor is sharing state
instead of cloning it. `AnyRateQuoter`'s copy constructor calls
`Concept::clone()`, which every `Model<Quoter>` implements by copying its
own `quoter_` member, so the clone is a real, independent `SurgeQuoter`
with its own counter.

`AnyRateQuoter` needs a hand-written copy constructor for a different
reason than PImpl needs one, a distinction worth drawing before this
chapter turns to PImpl itself. `Concept` is not an incomplete type the way
PImpl's `Impl` is: `Concept` and `Model` are fully defined right here in
this header; nothing about them is hidden across a translation-unit
boundary. The destructor and the move members can stay defaulted for
exactly that reason. The copy constructor needs hand-written code for an
entirely different cause: `model_` is a `std::unique_ptr<Concept>`, and a
`unique_ptr` is move-only by design, so the compiler-generated copy
constructor that would try to copy it does not exist in the first place.
Writing `clone()` and routing the copy constructor through it is not
firewall plumbing; it is working around owning a polymorphic object
through a smart pointer that refuses to be copied on its own.

## PImpl: pushing past the compilation firewall's mechanics

`RateQuoteLedger` hides every detail of how it stores and formats the
quotes it records behind a single `std::unique_ptr<Impl>`, the identical
shape Chapter 15 built for `LabelRenderer`: an out-of-line destructor,
because `std::default_delete<Impl>` needs `Impl`'s full definition to
destroy it and that definition only exists in `rate_quote_ledger.cpp`; a
hand-written copy constructor and copy assignment operator that deep-clone
`Impl` rather than copy the pointer; and defaulted move construction and
move assignment, because transferring ownership of a pointer never
requires seeing what it points to. Chapter 15 explains why each of those
five functions looks the way it does; the firewall they buy matters
beyond the recompiles it saves for a single consumer of the header.

The economic case gets sharper the moment `RateQuoteLedger` is imagined
shipping as part of a library other teams link against rather than as
source every consumer rebuilds from scratch. `RateQuoteLedger`'s public
layout is exactly one pointer, and it stays exactly one pointer no matter
how much bookkeeping `Impl` grows to hold: a running average, a
per-carrier breakdown, a timestamp on every entry, all of that can be
added to `Impl` in `rate_quote_ledger.cpp` without changing
`sizeof(RateQuoteLedger)` or the order of its public members by one byte.
That stability is what an ABI is: a promise about layout and calling
convention that already-compiled code depends on. A public class whose
layout changes between a library's point releases breaks every binary
compiled against the old layout, with no compiler diagnostic anywhere,
because that old binary keeps computing member offsets and `sizeof`
against a shape that no longer
matches the new library it links against at runtime. PImpl's one-pointer
layout is immune to that specific failure by construction, which is why
widely-shipped C++ libraries use it far more aggressively than a single
header-only tutorial example tends to show.

A second case applies here that `LabelRenderer` never ran into, because it
had no state to mutate by accident: `std::unique_ptr` does not propagate
constness to what it points to. A
`const` member function of `RateQuoteLedger`, say `summary() const`, holds
`impl_` as a `const std::unique_ptr<Impl>&` in effect, which only means
the pointer itself cannot be reseated; it says nothing about whether
`*impl_` can be mutated through it. Calling a non-`const` method on
`*impl_` from inside a `const` member function of `RateQuoteLedger`
compiles without complaint, because the compiler is checking constness on
the pointer, not forwarding it through to the pointee the way it would for
a plain `Impl` member or a reference. The discipline a PImpl type has to
maintain by convention, not by the type system, is keeping every `Impl`
method called from a `const` outer method itself marked `const`; nothing
in the type system stops a careless edit from breaking that promise.

The two idioms in this chapter also explain each other's boundaries.
`AnyRateQuoter` could never be rewritten behind a PImpl: its constructor
is a template, instantiated once per concrete `Quoter` type a caller
passes in, and a template's definition has to be visible everywhere it
gets instantiated, which is precisely the opposite of hiding it behind a
pointer to an incomplete type defined in one `.cpp` file. PImpl works on
`RateQuoteLedger` specifically because it is an ordinary, non-template
class with a fixed, nameable set of operations. The two techniques split
this chapter's domain along exactly that line: the piece that has to stay
open to an unbounded set of types uses type erasure, and the piece that
has a closed, stable interface over a changeable implementation uses
PImpl.

## A typed rejection: C++20 versus C++23

`RateQuoteLedger::record()` throws `std::invalid_argument` on a negative
amount, which works but forces every caller either to wrap the call in a
`try` block or accept that an unhandled exception can propagate out of a
checkout path. C++23's `std::expected` gives the same rejection a typed
alternative: a return type carrying either successful completion or a
specific reason the record was rejected, checkable with an `if` and no
exception machinery at all. `try_record()` in this example is guarded
behind `#if __cplusplus >= 202302L` precisely so the mandatory C++20 build
path never needs `std::expected`, while the C++23 build gets the more
precise contract.

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

Both versions call down into the identical `Impl::record()`; only how a
caller learns about a rejected amount changes between the two columns,
the same narrow scope Chapter 8's and Chapter 15's own codetabs held to
for their typed-error refinements.

> **AI sidebar.** Ask an LLM pairing partner to make several unrelated
> types "work the same way" and a common first move is inventing a shared
> abstract base class for them, an `IQuoter` interface each one is made to
> inherit from. That reflex breaks the moment any of those types is not
> yours to change: a quoter pulled in from a third-party rating library, a
> plain struct another team owns, a lambda with no class to retrofit
> inheritance onto. Check whether the generated interface requires editing
> types you do not control before accepting it; if it does, a concept/model
> pair reaching the same uniform handling without touching a single one of
> those types is the smaller answer that works without them. The matching
> check on generated PImpl code is narrower but just as concrete: does the
> generated class compile the moment you add the `std::unique_ptr<Impl>`
> member, or only once you separately remember the out-of-line destructor?
> A generated header with the pointer but no destructor declared is a
> linker error waiting in whatever `.cpp` file first needs the implicit one,
> and it is worth building immediately rather than trusting that a header
> compiles clean just because it parses clean.

## When not to use type erasure or PImpl

Type erasure earns its cost when the concrete types it needs to unify are
not under your control to make inherit from anything, or when a caller
specifically needs an owned, copyable value rather than a pointer into a
class hierarchy someone has to manage. Four `Quotable` types sharing a
common library and a common team that could cheaply add a `RateStrategy`
base class to all four do not need `AnyRateQuoter`; Chapter 8's plain
interface-and-`unique_ptr` Strategy already solves that case with less
machinery, and a concept/model pair sitting beside a base class that could
have done the same job is a second abstraction with nothing distinct
underneath it. The signal worth watching for, the same one the AI sidebar
names, is whether the types being erased are out of your hands to modify;
if they are not, a shared interface is the plainer, equally correct
answer.

PImpl earns its cost when a class's implementation detail is large,
volatile, or drags in a dependency the rest of the codebase should not
have to include transitively, the same bar Chapter 15 set. A class with
one `int` member and no churn gains nothing from PImpl beyond the
maintenance burden of five hand-written special member functions for a
firewall protecting nothing of consequence. And, as this chapter's own
domain shows directly, a class whose public operations are templates
cannot be put behind PImpl at all; reaching for the idiom on a type like
`AnyRateQuoter` is not a bad trade, it is a type error, because the
template bodies PImpl would need to hide have to stay visible at every
instantiation site by the language's own rules.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/22-type-erasure-pimpl` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 12 cases under the `cpp23` preset,
10 under `cpp20`, the difference being the two `std::expected`-specific
cases on `RateQuoteLedger::try_record()` that only compile under C++23.
Observed locally on Fedora 44 / clang 22.1.8, building this example
standalone from its own directory:

```
100% tests passed, 0 tests failed out of 12   # cpp23
100% tests passed, 0 tests failed out of 10   # cpp20
```

To see both idioms produce real output rather than just reading an
assertion that says they work, run the example's own demo script:

```sh
examples/22-type-erasure-pimpl/demo.sh cpp23
```

It stores three unrelated quoters in one `std::vector<AnyRateQuoter>`,
proves a copy's mutable state is independent of its original, and records
every quote into a `RateQuoteLedger` that still copies deeply after
callers have already recorded into it:

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

Notice the original ledger's entry count stays at 3 after
`ledger_copy.record()` adds a fourth entry only to the copy. That is the
PImpl copy constructor's deep clone showing up as an observable fact at
the terminal, the same check Chapter 15's `LabelRenderer` example made
for its own copy. Running
`examples/22-type-erasure-pimpl/demo.sh cpp20` prints the same output
minus the final `std::expected` block, which only compiles under the
`cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design
  Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480)
  (Addison-Wesley): Bridge's decoupling of an abstraction from its
  implementation is the structural ancestor PImpl specializes for a single
  concrete class, which is why Chapter 15's own Bridge section and this
  chapter's PImpl section build on the same entry. Type erasure is not a
  GoF-catalogued pattern; it is a C++ idiom drawn from how the standard
  library implements `std::function` and `std::any` themselves.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind
  `examples/22-type-erasure-pimpl`'s suite, which asserts that
  `AnyRateQuoter` dispatches correctly to several distinct concrete types
  and that a copy's mutable state stays independent, and that
  `RateQuoteLedger` copies deeply, moves correctly, and exposes no `Impl`
  detail through its own size, rather than only checking that each binary
  exits zero.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
