---
title: "Chain of Responsibility and Iterator"
order: 19
part: "coordinating-traversing"
description: "Two patterns for coordinating a collection of objects rather than one: Chain of Responsibility routes a request through a sequence of handlers until one of them acts, and Iterator provides sequential access to an aggregate without exposing its internal structure, taught with a shipment-approval chain and a zone-bucketed package manifest, including the C++20 ranges form of traversal."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force Chain of Responsibility answers: a request that must be
  checked against a sequence of conditions owned by different concerns,
  where the sender should not need to know which condition, if any, ends
  up acting on it.
- Build a chain of handlers where each one either decides a request or
  passes it along, and a fallback at the end guarantees the chain always
  terminates in a decision.
- Tell Chain of Responsibility apart from Decorator even though both wrap
  one object around another: one stops at the first handler that acts, the
  other calls every layer on every request.
- Build a hand-written forward iterator that provides sequential access to
  an aggregate without exposing how that aggregate stores its data
  internally.
- Express the same traversal as a C++20 ranges pipeline over the identical
  aggregate, and say what a ranges pipeline buys you that a raw loop does
  not.
- Recognize when either pattern is solving a problem a plain loop or a
  flat container already solves, and say what to write instead.

## Why this matters

A logistics platform rarely approves a shipment with one check. It has to
confirm the parcel is not over the carrier's weight limit, confirm a
hazardous-materials parcel is cleared for its destination country,
confirm the destination is not under an active trade embargo, and only
when none of that rejects the request outright, route anything left over
to a human reviewer. Each of those checks is owned by a different concern:
compliance owns the embargo list, safety owns the hazmat clearance list,
and operations owns the weight limit, and each one changes on its own
schedule, independent of the others. The code that submits a shipment for
approval should not have to know how many checks exist today, in what
order they run, or which team owns the rule that eventually accepts or
rejects a given request. Chain of Responsibility is the Gang of Four's
name for exactly that force: a request passed down a sequence of handlers,
each of which either acts on it or hands it to the next, so the sender
holds nothing more than a reference to the first handler in the chain.

A warehouse scanning app that lists packages has a different, unrelated
problem with a similar shape. The packages in a manifest are not
necessarily stored as one flat list. In this chapter's example a warehouse
sorts arriving packages into one lane per destination zone as they come
in, so the manifest holds four separate buckets internally, not a single
vector. A caller that wants to walk every package in zone order,
regardless of the order packages arrived, should not have to know the
manifest is bucketed at all. Iterator is the Gang of Four's name for that
force: a small object that knows how to traverse an aggregate's actual
structure and presents a uniform, sequential view of it to anything that
asks for one. Modern C++ gives this pattern a second, complementary
expression: once an aggregate's iterator satisfies the standard library's
iterator concepts, the same aggregate is also a range, and a `std::views`
pipeline can filter and transform it with no custom traversal code at all.

This chapter pairs the two patterns because they sit next to each other in
this book's catalogue of coordinating structures, not because one grows
out of the other. Chain of Responsibility coordinates a sequence of
*decisions*; Iterator coordinates a sequence of *elements*. Reading them
together is also where a reader builds the habit this book has trained
since Chapter 1: naming the actual force at work, a request that needs
routing versus a structure that needs walking, before reaching for either
pattern's shape.

{% include excalidraw.html name="19-chain-iterator" caption="Figure 19.1: A ShipmentRequest flowing down a chain of handlers that each handle or pass, next to a PackageManifest traversed by a hand-written forward iterator and by a views::filter plus views::transform ranges pipeline over the same bucketed data." %}

## The domain: a shipment-approval chain and a zone-bucketed manifest

The example behind this chapter lives in `examples/19-chain-iterator` and
splits into the two halves the diagram shows. The Chain of Responsibility
half works against a small request type:

```cpp
struct ShipmentRequest {
    double weight_kg;
    bool hazmat;
    std::string destination_country;
};
```

Every handler in the chain shares one interface, `ApprovalHandler`, whose
public `handle()` method does the actual routing:

```cpp
class ApprovalHandler {
public:
    explicit ApprovalHandler(std::string name, std::unique_ptr<ApprovalHandler> next = nullptr);
    virtual ~ApprovalHandler() = default;

    ApprovalDecision handle(const ShipmentRequest& request, std::vector<std::string>& call_log) const;

protected:
    virtual std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const = 0;

private:
    std::string name_;
    std::unique_ptr<ApprovalHandler> next_;
};
```

The Iterator half works against a different aggregate, `PackageManifest`,
which doesn't store its `Package` values as one flat sequence. It stores
four buckets, one per `DestinationZone` (`Local`, `Regional`, `National`,
`International`), because that is how a warehouse sorts inbound
packages. `PackageManifest::const_iterator` is the object whose entire job
is hiding that bucketing from anything that walks the manifest.

## Chain of Responsibility: a handler either acts or passes

`ApprovalHandler::handle()` is the one method every caller of this chain
ever calls, and it does the same three things on every handler in the
chain: record that this handler was consulted, ask the handler's own
`try_handle()` whether it has a decision, and if not, delegate to the next
handler.

```cpp
ApprovalDecision ApprovalHandler::handle(const ShipmentRequest& request,
                                          std::vector<std::string>& call_log) const {
    call_log.push_back(name_);
    if (auto decision = try_handle(request)) {
        return *decision;
    }
    if (next_) {
        return next_->handle(request, call_log);
    }
    return ApprovalDecision{ApprovalOutcome::NeedsManualReview, name_,
                             "no handler in the chain reached a decision"};
}
```

Three concrete handlers each own exactly one check. `WeightLimitHandler`
rejects a shipment over its configured limit; `HazmatHandler` rejects a
hazardous shipment headed to a country not on its cleared list;
`EmbargoHandler` rejects a shipment to a destination under embargo. Each
one's `try_handle()` returns a decision only when its own rule fires,
and `std::nullopt` otherwise, which is the signal `handle()` reads as
"pass this to the next handler":

```cpp
std::optional<ApprovalDecision> HazmatHandler::try_handle(const ShipmentRequest& request) const {
    if (!request.hazmat) {
        return std::nullopt;
    }
    const bool cleared = std::find(cleared_countries_.begin(), cleared_countries_.end(),
                                    request.destination_country) != cleared_countries_.end();
    if (!cleared) {
        return ApprovalDecision{ApprovalOutcome::Rejected, name(),
                                 "hazmat shipments are not cleared for " + request.destination_country};
    }
    return std::nullopt;
}
```

`build_default_chain()` wires the four handlers together, innermost
construction first, the same ownership shape Chapter 16 used to compose a
Decorator stack:

```cpp
std::unique_ptr<ApprovalHandler> build_default_chain() {
    auto fallback = std::make_unique<ManualApprovalHandler>();
    auto embargo = std::make_unique<EmbargoHandler>(std::vector<std::string>{"KP", "IR"}, std::move(fallback));
    auto hazmat = std::make_unique<HazmatHandler>(std::vector<std::string>{"US", "CA"}, std::move(embargo));
    return std::make_unique<WeightLimitHandler>(40.0, std::move(hazmat));
}
```

A caller holding only the `WeightLimitHandler` returned from
`build_default_chain()` can't tell, from that single `handle()` call,
whether a request stopped at the first handler or traveled through all
four. `tests/approval_chain_test.cpp` makes that observable directly by
asserting the full `call_log` alongside the final decision: a 55 kg
shipment produces a `call_log` of exactly `{"WeightLimitHandler"}`,
confirming that `HazmatHandler` and `EmbargoHandler` are not consulted
once `WeightLimitHandler` has already rejected the request.

## The fallback guarantees the chain terminates

`ManualApprovalHandler` sits at the end of the chain with no next handler,
and its `try_handle()` never returns `std::nullopt`:

```cpp
std::optional<ApprovalDecision> ManualApprovalHandler::try_handle(const ShipmentRequest& /*request*/) const {
    return ApprovalDecision{ApprovalOutcome::NeedsManualReview, name(), "routed to the manual review queue"};
}
```

Without the fallback, nothing in the chain's own type guarantees a
request ever reaches a decision. A chain built by hand, by stringing
`unique_ptr` constructors together, has no language-level check that some
handler eventually decides a request; if every concrete handler returns
`std::nullopt` and the chain runs out of handlers, `handle()` falls
through its `if (next_)` branch into the final return statement, which
reports that case as an explicit `NeedsManualReview` rather than leaving
the caller to guess. Designing one handler, the fallback, whose
`try_handle()` is unconditional, turns "falls off the end unhandled" into
"always lands in a known, named outcome." A shipment to Germany, not
overweight, not hazmat, not embargoed, clears all three automatic checks
and reaches `ManualApprovalHandler`, which routes it to a human reviewer
rather than approving it automatically. Nothing in this domain is
approved without review: the fallback's job is routing to a human, not
approving on the chain's behalf.

## Chain of Responsibility versus Decorator

Both this chapter's chain and Chapter 16's `RateSource` decorator stack
wrap one object's interface around another and let a caller hold only the
outermost layer. The two patterns answer opposite questions about how many
layers run. A Decorator
stack always calls every layer on every request: `SurchargeRateSource`
always calls down into whatever it wraps and adds a markup on top, no
matter what the wrapped source returns. A Chain of Responsibility stops at
the first handler that acts, and nothing downstream of that handler
ever sees the request at all. `tests/approval_chain_test.cpp`'s
`call_log` assertions exist specifically to make that difference
observable: a chain's log for a rejected request is short, a decorator
stack's call sequence for any request is always the full stack. If a
reviewer can't immediately say whether a given layer in a new design
always runs or only runs until one layer claims the request, that
question, not the two patterns' superficially similar constructor shapes,
is the one to answer first.

## Iterator: hiding the manifest's bucketed storage

`PackageManifest` stores its packages in four buckets, one per zone, and
`add()` routes each new package into the bucket matching its zone:

```cpp
void PackageManifest::add(Package package) {
    buckets_by_zone_[static_cast<std::size_t>(package.zone)].push_back(std::move(package));
}
```

A caller that wants to list every package in the manifest, in zone order,
should not have to know buckets exist. `PackageManifest::const_iterator`
is a hand-written forward iterator, modeled on the standard library's own
iterator shape, that flattens the four buckets into one traversal:

```cpp
class const_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Package;
    using difference_type = std::ptrdiff_t;
    using pointer = const Package*;
    using reference = const Package&;

    reference operator*() const;
    const_iterator& operator++();
    bool operator==(const const_iterator& other) const = default;
    // ...
};
```

`operator++()` advances one position within the current bucket, then
calls a private `skip_empty_buckets()` helper that moves to the next
nonempty bucket whenever the current one runs out, continuing until it
finds a package or reaches the end of the last bucket:

```cpp
void PackageManifest::const_iterator::skip_empty_buckets() {
    while (buckets_ != nullptr && bucket_index_ < buckets_->size() &&
           position_ >= (*buckets_)[bucket_index_].size()) {
        ++bucket_index_;
        position_ = 0;
    }
}
```

`begin()` constructs an iterator at bucket zero, position zero, and
`end()` constructs one with `bucket_index_` already past the last bucket;
both call `skip_empty_buckets()` in their constructor, so an empty first
bucket never produces a dereferenceable-but-wrong iterator. Because
`skip_empty_buckets()` is a deterministic function of position, not of
which iterator asked, two iterators that have each advanced to the same
logical element end up with identical `bucket_index_` and `position_`
values, which is exactly what the defaulted `operator==` needs to compare
correctly. A caller holding a `PackageManifest::const_iterator` has no way
to observe that the manifest underneath it is four vectors rather than
one; `operator*()`, `operator++()`, and `operator==()` are the entire
contract, and `src/package_manifest.cpp` asserts the type satisfies the
standard library's concept for it:

```cpp
static_assert(std::forward_iterator<PackageManifest::const_iterator>);
static_assert(std::ranges::forward_range<PackageManifest>);
```

The second assertion matters more than the first. Because
`PackageManifest` exposes `begin()` and `end()` returning a type that
satisfies `std::forward_iterator`, the manifest itself satisfies
`std::ranges::forward_range` with no extra code and no
`std::ranges::subrange` wrapping. A `PackageManifest` interoperates with
`std::find_if` exactly the way a `std::vector` would, and with a `for`
loop that never once mentions buckets:

```cpp
for (const Package& package : manifest) {
    std::cout << package.tracking_number << '\n';
}
```

## The same traversal as a ranges pipeline

A hand-written iterator answers "how do I traverse this," one element at a
time. A ranges pipeline answers a related but different question, "what
subset of this, transformed how," declaratively, and it only works at all
because `PackageManifest` is already a `forward_range`. `std::views::filter`
and `std::views::transform` compose directly over the manifest, with no
awareness that packages live in zone buckets underneath:

```cpp
auto national_weights =
    manifest | std::views::filter([](const Package& p) { return p.zone == DestinationZone::National; }) |
    std::views::transform([](const Package& p) { return p.weight_kg; });
```

Nothing about `national_weights` has copied a single `Package` out of the
manifest yet. `views::filter` and `views::transform` are lazy: the
predicate and the projection run only when something iterates the
resulting view, which is exactly what `tests/package_manifest_test.cpp`
does when it copies the view's elements into a `std::vector<double>` for
comparison. The custom iterator this chapter wrote by hand is still doing
all of the real traversal work underneath this pipeline. The pipeline adds
nothing to how a `PackageManifest` is walked; it adds a declarative way to
describe which elements matter and what to do with them, stacked on top of
a traversal mechanism the iterator chapter's half of this example already
built.

## A ranges refinement: C++20 versus C++23

Both standard-library versions in this chapter's codetabs build the
identical filtered-and-transformed view over the identical manifest. What
changes between them is how that view becomes a concrete, owned
`std::vector` a caller can keep past the manifest's lifetime. C++20 has no
single standard-library call that does this; a caller writes a small loop,
or constructs a `std::vector` from the view's `begin()`/`end()` pair
directly. C++23 adds `std::ranges::to`, which collects a range into any
container type in one expression, at the end of the same pipe chain the
view was already built from:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: no std::ranges::to yet. Construct the vector directly from the
// view's begin()/end() pair, or build it with an explicit loop.
auto view = manifest | std::views::filter([](const Package& p) { return p.zone == DestinationZone::National; }) |
            std::views::transform([](const Package& p) { return p.tracking_number; });
std::vector<std::string> national_tracking_numbers(view.begin(), view.end());
```
```cpp
// C++23: std::ranges::to collects the pipeline straight into a vector, as
// one more stage of the same pipe chain.
auto national_tracking_numbers =
    manifest | std::views::filter([](const Package& p) { return p.zone == DestinationZone::National; }) |
    std::views::transform([](const Package& p) { return p.tracking_number; }) |
    std::ranges::to<std::vector>();
```

Both versions produce the identical `std::vector<std::string>`, and
`tests/package_manifest_test.cpp`'s C++23-guarded case asserts exactly
that. The view itself, `filter` composed with `transform` over the same
`PackageManifest`, is unchanged between the two columns; only the last
step, turning a lazy view into an owned container, gained a dedicated
standard-library name.

> **AI sidebar.** Ask an LLM pairing partner to "add a check" to an
> existing chain of validations, and a common result is a new `if`
> statement inserted directly into whatever function already runs the
> other checks, growing one function instead of adding one handler. That
> shape works today and breaks the property this chapter's
> `call_log` assertions exist to protect: a reviewer can no longer tell,
> without re-reading the whole function, whether a new check can run
> before or after an existing one, or whether it is supposed to stop
> processing when it fires. The same prompt aimed at this chapter's chain
> should produce one new `ApprovalHandler` subclass, inserted at a named
> position in `build_default_chain()`, with its own `try_handle()`
> returning `std::nullopt` on every path that should pass to the next
> handler. Check which shape a generated change took before accepting it.
> A new `if` branch growing inside `ApprovalHandler::handle()` itself, or
> inside the function that calls it, is the sign the model reached for the
> shortest diff instead of the one this chapter's forces call for.

## When not to use Chain of Responsibility or Iterator

Chain of Responsibility earns its handler objects when the number of
checks is open-ended, owned by more than one team, or needs to
change which checks run and in what order without touching a shared
function. A fixed set of two or three checks, all owned by one team, that
never needs to reorder or disable a check independently of the others does
not need `ApprovalHandler` subclasses and a `build_default_chain()`
function; it needs an `if`/`else if` chain inside one function, read start
to finish by whoever maintains it. The moment that calculus flips is the
moment a second team needs to add a check without touching the first
team's code, or a check needs to be swapped out per deployment region at
runtime. Until then, the plain branch is the stronger engineering choice,
even with this chapter's handler classes sitting right there as a
template to copy.

Iterator fails a related test in a narrower way. If `PackageManifest`
stored its packages as one flat `std::vector<Package>` instead of four
zone buckets, this chapter's hand-written `const_iterator` would have
nothing to hide, because `std::vector<Package>::const_iterator` already
provides sequential access with no internal structure to leak in the first
place. Writing a custom iterator class around a container that is already
flat adds an indirection layer, every `operator*()` and `operator++()`
call forwarding to the wrapped vector's own, with nothing behind it to
justify the cost. The reason this example's iterator is worth writing by
hand is specifically that the manifest's real storage, four zone buckets,
is not what a caller should see; reach for a custom iterator only when
there is a real structure like that one to hide, and reach for the
aggregate's native container iterator directly otherwise. A ranges
pipeline doesn't change that calculus either: `views::filter` and
`views::transform` are worth composing when a caller needs a declarative
subset-and-project view over a range, not as a way to dress up a single
pass that a plain `for` loop already expresses clearly.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/19-chain-iterator` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 9 cases under the `cpp23` preset, 8
under `cpp20`, the difference being the one `std::ranges::to` case that
only compiles under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 9   # cpp23
100% tests passed, 0 tests failed out of 8   # cpp20
```

To see the approval chain and the manifest iterator produce real output
rather than just reading an assertion that says they work, run the
example's own demo script:

```sh
examples/19-chain-iterator/demo.sh cpp23
```

It routes four shipment requests through the chain, printing which handler
decided each one and the full `call_log` of handlers consulted along the
way, then walks the same `PackageManifest` through the hand-written
iterator and the ranges pipeline:

```
== Chain of Responsibility: shipment approval ==
US (55 kg): Rejected by WeightLimitHandler -- shipment weight 55 kg exceeds the 40 kg limit
  chain visited: WeightLimitHandler
FR (10 kg, hazmat): Rejected by HazmatHandler -- hazmat shipments are not cleared for FR
  chain visited: WeightLimitHandler -> HazmatHandler
KP (10 kg): Rejected by EmbargoHandler -- destination KP is under embargo
  chain visited: WeightLimitHandler -> HazmatHandler -> EmbargoHandler
DE (10 kg): Needs manual review by ManualApprovalHandler -- routed to the manual review queue
  chain visited: WeightLimitHandler -> HazmatHandler -> EmbargoHandler -> ManualApprovalHandler

== Iterator: a hand-written forward iterator over a bucketed manifest ==
Inserted in INTL, LOC, NAT, NAT, REG order; the iterator still visits zone order:
  TRK-LOC-1
  TRK-REG-1
  TRK-NAT-1
  TRK-NAT-2
  TRK-INTL-1
std::find_if locates the first Regional package: TRK-REG-1

== Iterator: the same data through a ranges filter + transform pipeline ==
National-zone weights (kg): 5 7.5

== C++23 refinement: std::ranges::to collects the pipeline directly ==
National tracking numbers: TRK-NAT-1 TRK-NAT-2
```

Notice the first four lines of the chain output: each `call_log` is
exactly as long as the number of handlers that ran, one for the
overweight US shipment, two for the hazmat shipment to France, three for
the embargoed shipment to North Korea, and all four only for the shipment
to Germany that clears every automatic check. That growing log is the
chain stopping at the first handler that acts, shown as an observable
fact rather than inferred from the final decision alone. The manifest
section shows the opposite fact: packages inserted in International,
Local, National, National, Regional order come back out in zone order,
Local first and International last, because the iterator walks buckets by
zone regardless of insertion order. Running
`examples/19-chain-iterator/demo.sh cpp20` prints the same output minus
the final `std::ranges::to` block, which only compiles under the `cpp23`
preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design
  Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley):
  the original Chain of Responsibility and Iterator entries, including the
  "avoid coupling the sender of a request to its receiver" and "provide a
  way to access elements sequentially without exposing its underlying
  representation" intent statements this chapter's approval chain and
  package manifest each build on.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official
  GoogleTest documentation: the testing idiom behind
  `examples/19-chain-iterator`'s suite, which asserts the full chain of
  handlers consulted for a request, not only the final decision, and
  compares a ranges pipeline's output against a plain loop over the same
  manifest.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
