---
title: "Chain of Responsibility and Iterator"
order: 19
part: "coordinating-traversing"
description: "Two patterns for coordinating a collection of objects rather than one: Chain of Responsibility routes a request through a sequence of handlers until one of them acts, and Iterator provides sequential access to an aggregate without exposing its internal structure, taught with a shipment-approval chain and a zone-bucketed package manifest, including the C++20 ranges form of traversal."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Route a request through a sequence of checks owned by different concerns, so the sender never knows which check acted.
- Build a chain where each handler decides or passes, and a fallback guarantees a decision.
- Tell Chain of Responsibility from Decorator: one stops at the first handler that acts, the other runs every layer.
- Write a forward iterator that walks an aggregate without exposing how it stores its data.
- Express the same traversal as a ranges pipeline, and say what it adds over a raw loop.
- Recognize when a plain branch, loop or flat container already solves the problem.

## Why this matters

A logistics platform approves a shipment with several checks: the parcel is under the carrier's weight limit, a hazardous parcel is cleared for its destination, the destination isn't under embargo, and anything left goes to a human reviewer. Operations owns the weight limit, safety the hazmat list and compliance the embargo list, and each changes on its own schedule. The code that submits a shipment shouldn't know how many checks exist, their order, or which one decided.

A warehouse manifest has a different problem with a similar shape. Arriving packages are sorted into one lane per destination zone, so the manifest holds four buckets, not one vector. A caller that walks every package in zone order shouldn't know about the buckets. Once the manifest's iterator satisfies the standard iterator concepts, the manifest is also a range, and `std::views` can filter and transform it with no custom traversal code.

Chain of Responsibility coordinates a sequence of decisions; Iterator coordinates a sequence of elements.

{% include excalidraw.html name="19-chain-iterator" caption="Figure 19.1: A ShipmentRequest flowing down a chain of handlers that each handle or pass, next to a PackageManifest traversed by a hand-written forward iterator and by a views::filter plus views::transform ranges pipeline over the same bucketed data." %}

## Pattern: Chain of Responsibility

### The problem

Every approval request carries the same fields:

```cpp
struct ShipmentRequest {
    double weight_kg;
    bool hazmat;
    std::string destination_country;
};
```

Putting all the checks in one function couples the submitter to every rule and every team. Adding or reordering a check means editing shared code.

### The solution

**A Chain of Responsibility passes a request along a sequence of handlers until one acts on it, so the sender holds only the first handler.**

### How C++ models it

Every handler shares `ApprovalHandler`:

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

`handle()` is a non-virtual routing step: record the handler, ask `try_handle()`, and delegate if there's no decision:

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

Each concrete handler owns one check: `WeightLimitHandler`, `HazmatHandler` and `EmbargoHandler`. `try_handle()` returns a decision when its rule fires and `std::nullopt` to pass:

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

`build_default_chain()` wires the four handlers, innermost first, each owning the next by `std::unique_ptr`:

```cpp
std::unique_ptr<ApprovalHandler> build_default_chain() {
    auto fallback = std::make_unique<ManualApprovalHandler>();
    auto embargo = std::make_unique<EmbargoHandler>(std::vector<std::string>{"KP", "IR"}, std::move(fallback));
    auto hazmat = std::make_unique<HazmatHandler>(std::vector<std::string>{"US", "CA"}, std::move(embargo));
    return std::make_unique<WeightLimitHandler>(40.0, std::move(hazmat));
}
```

A caller holding the `WeightLimitHandler` can't tell whether a request stopped there or traveled all four handlers. `call_log` makes it visible: a 55 kg shipment logs only `WeightLimitHandler`, and the hazmat and embargo checks never see it.

### The fallback

`ManualApprovalHandler` sits at the end with no next handler, and its `try_handle()` never passes:

```cpp
std::optional<ApprovalDecision> ManualApprovalHandler::try_handle(const ShipmentRequest& /*request*/) const {
    return ApprovalDecision{ApprovalOutcome::NeedsManualReview, name(), "routed to the manual review queue"};
}
```

Nothing in a hand-built chain's type guarantees a decision. Without the fallback, a chain that runs out of handlers drops into the final return in `handle()`, which still reports `NeedsManualReview` rather than leaving the caller to guess. With it, every request lands in a named outcome. A 10 kg shipment to Germany clears all three automatic checks and goes to a human reviewer: nothing in this domain is approved automatically.

### Chain or Decorator

Both wrap one object's interface around another and let the caller hold only the outermost layer. They differ in how many layers run.

| | Chain of Responsibility | Decorator |
| --- | --- | --- |
| Layers per request | Up to the first that acts | All of them |
| Downstream of the deciding layer | Never sees the request | Always called |
| Each layer adds | A decision or a pass | Behavior around the inner call |

If you can't say whether a new layer always runs or runs only until someone claims the request, answer that before picking a shape.

## Pattern: Iterator

### The problem

`PackageManifest` stores packages in four buckets, one per `DestinationZone` (`Local`, `Regional`, `National`, `International`), and `add()` routes each package to its bucket:

```cpp
void PackageManifest::add(Package package) {
    buckets_by_zone_[static_cast<std::size_t>(package.zone)].push_back(std::move(package));
}
```

Callers want every package in zone order, and exposing the buckets would tie them to the storage.

### The solution

**An Iterator gives sequential access to an aggregate's elements without exposing its underlying representation.**

### How C++ models it

`PackageManifest::const_iterator` is a hand-written forward iterator that flattens the buckets:

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

`operator++()` advances within the current bucket, then a private helper moves past any exhausted or empty bucket:

```cpp
void PackageManifest::const_iterator::skip_empty_buckets() {
    while (buckets_ != nullptr && bucket_index_ < buckets_->size() &&
           position_ >= (*buckets_)[bucket_index_].size()) {
        ++bucket_index_;
        position_ = 0;
    }
}
```

`begin()` starts at bucket zero, `end()` starts past the last bucket, and both constructors call `skip_empty_buckets()`, so an empty first bucket never yields a bad iterator and an empty manifest's `begin()` equals its `end()`. Because the helper depends only on position, two iterators at the same element hold identical `bucket_index_` and `position_`, which is what the defaulted `operator==` needs.

### The manifest is a range

`src/package_manifest.cpp` pins the contract:

```cpp
static_assert(std::forward_iterator<PackageManifest::const_iterator>);
static_assert(std::ranges::forward_range<PackageManifest>);
```

The second assertion matters more. With `begin()` and `end()` returning a `std::forward_iterator`, the manifest is a `std::ranges::forward_range` with no extra code. It works with `std::find_if` as a `std::vector` would, and with a loop that never mentions buckets:

```cpp
for (const Package& package : manifest) {
    std::cout << package.tracking_number << '\n';
}
```

### The same traversal as a pipeline

The iterator answers "how do I walk this". A ranges pipeline answers "which elements, transformed how", and it composes over the manifest only because the manifest is a range:

```cpp
auto national_weights =
    manifest | std::views::filter([](const Package& p) { return p.zone == DestinationZone::National; }) |
    std::views::transform([](const Package& p) { return p.weight_kg; });
```

The views are lazy: the predicate and projection run only when something iterates `national_weights`. The hand-written iterator still does all the traversal underneath.

### Collecting into a container

Turning the lazy view into an owned `std::vector` differs between standards. C++20 constructs the vector from the view's iterator pair; `std::ranges::to`, new in C++23, collects it as one more pipe stage:

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

Both produce the same `std::vector<std::string>`; only the last step changed.

## When not to use it

- **Chain of Responsibility.** Two or three checks owned by one team that never reorder belong in an `if`/`else if` inside one function. Switch to handlers when a second team must add a check without touching the first team's code, or when checks must change per deployment region at runtime.
- **Iterator.** If `PackageManifest` were one flat `std::vector<Package>`, its `const_iterator` would already give sequential access, and a custom iterator would only forward `operator*()` and `operator++()`. Write one only when there's a real structure to hide.
- **Ranges pipeline.** Compose `views::filter` and `views::transform` when a caller needs a subset-and-project view. A single pass that a plain `for` loop states clearly doesn't need one.

> **🤖 Explore with your agent**
>
> "Add a `DeclaredValueHandler` deriving from `ApprovalHandler` that rejects a `ShipmentRequest` above a configured limit. Implement only `try_handle()`, returning `std::nullopt` on every path that should pass. Insert it in `build_default_chain()` between `HazmatHandler` and `EmbargoHandler`, and leave `ApprovalHandler::handle()` and `ManualApprovalHandler` unchanged."
>
> Run `ctest --preset cpp23 -R ApprovalChain`, and check that `ApprovalHandler::handle()` gained no new branch.

## Build, run, observe

From the repository root, the superbuild picks up `examples/19-chain-iterator`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 9 cases pass under `cpp23` and 8 under `cpp20`; the `std::ranges::to` case compiles only under C++23. To see the chain and the iterator run:

```sh
examples/19-chain-iterator/demo.sh cpp23
```

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

Each `chain visited` line is as long as the number of handlers that ran: one for the overweight shipment, four for the one that reaches manual review. Packages inserted out of zone order come back Local first and International last. Under `cpp20` the output stops before the `std::ranges::to` block.

- **Stop skipping buckets on increment.** In a scratch copy, delete the `skip_empty_buckets();` call in `const_iterator::operator++()` in `src/package_manifest.cpp`. Three of the 9 cases crash with a segmentation fault: the zone-order walk and both pipelines read past the end of a bucket.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Chain of Responsibility and Iterator entries, including the "avoid coupling the sender of a request to its receiver" and "provide a way to access elements sequentially without exposing its underlying representation" intent statements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/19-chain-iterator`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
