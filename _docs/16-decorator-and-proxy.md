---
title: "Decorator and Proxy"
order: 16
part: "composing-adapting-structure"
description: "Two structural patterns that wrap an object behind its own interface, taught against a carrier rate pipeline and a manifest archive: Decorator stacks layers that each add one responsibility in an order that changes the result, and Proxy stands a single surrogate in front of an expensive or access-controlled subject without adding any new capability."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force both patterns answer: an object wrapped behind the exact
  interface it already implements, so a caller holding that interface
  cannot tell how many wrappers, if any, sit between it and the real thing.
- Build a Decorator stack where each layer adds one responsibility, and
  show that the order the layers wrap in is part of the design, not an
  implementation detail.
- Build a lazy-initialization Proxy that defers constructing an expensive
  real subject until the first call reaches it, and an access-control
  Proxy that gates a call on a policy check.
- Tell Decorator and Proxy apart by intent even though both wrap the same
  interface they stand in front of: one adds behavior and is meant to
  stack, the other controls access and is meant to be transparent.
- Choose between throwing on a denied access check and C++23's
  `std::expected` for the same check, and say which standard each belongs
  to.
- Recognize when either pattern is wrapping an object that has nothing to
  wrap it for, and say what to write instead.

## Why this matters

A checkout pipeline that quotes a shipping rate rarely wants exactly one
thing to happen when it asks for a quote. It wants the quote computed, and
it wants a surcharge added for high-value shipments, and it wants the
computation cached so a cart that gets priced three times in one session
does not hit the tariff table three times, and it wants every quote logged
for the next billing dispute. None of those four concerns are the
quote itself. Each one is something that happens *around* the quote, and
each one is the kind of thing a team ships on its own schedule: the
surcharge rule changes when finance renegotiates a contract, the cache
gets added once load testing shows the tariff lookup is hot, and the
logging gets added when support asks for an audit trail. Decorator is the
Gang of Four's name for building a pipeline like that out of layers that
each implement the same interface the thing they wrap implements, so any
combination of them, in any order a caller chooses to assemble, still
looks like one quote source to whatever code asks it for a price.

A warehouse scanning app has a different problem with the same shape. It
needs to look up a carrier's manifest for a tracking number, and the real
manifest archive is expensive to open: a full day's manifests, loaded off
cold storage, before the first lookup can even run. The app also has more
than one kind of user hitting that lookup, and a guest account has no
business reading a manifest that a warehouse worker's badge unlocks.
Neither problem is about the manifest archive's own logic. One is
about *when* the real archive gets built, and the other is about *whether*
a particular caller is allowed to reach it at all. Proxy is the Gang of
Four's name for resolving that force: a surrogate that implements the
exact interface the real subject implements, and that a caller cannot
distinguish from the real thing except by the one behavior the proxy
exists to change, construction timing or access control, never what a
successful call returns.

Both patterns wrap an object behind its own interface. That similarity is
exactly why this book pairs them in one chapter: the test for telling them
apart is not what either one looks like in code, which can be nearly
identical, but what each one is *for*.

{% include excalidraw.html name="16-decorator-proxy" caption="Figure 16.1: A Decorator stack (surcharge, caching, logging) wrapping a core RateSource, next to a Proxy (AccessControlledManifestProxy) controlling access to a real ManifestStore subject. Both wrap the same interface their subject implements; only the intent differs." %}

## The domain: a rate pipeline and a manifest archive

The example behind this chapter lives in `examples/16-decorator-proxy`, and
it splits cleanly into the two halves the diagram shows. The Decorator half
reuses this book's familiar `Shipment` shape, a `weight_kg`, a
`DestinationZone`, and a `declared_value_cents`, and a single shared
interface every layer in a rate pipeline implements:

```cpp
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) = 0;
};
```

`TariffTableRateSource` is the core component at the bottom of every stack
in this example: a plain per-zone, per-kilogram lookup with no decoration
at all. Everything else in the Decorator half of this chapter wraps
something that implements `RateSource`, and ultimately, at the bottom of
any real stack, that something is this class.

The Proxy half uses a separate interface for a separate concern, a
carrier's manifest archive:

```cpp
class ManifestStore {
public:
    virtual ~ManifestStore() = default;
    virtual std::string lookup(const std::string& tracking_number) = 0;
};
```

`ArchivedManifestStore` is the real subject, standing in for a load that in
production would mean pulling a day's manifests off cold storage before a
single tracking number resolves. Both halves of this example share one
property: a client holding a `RateSource&` or a `ManifestStore&` can never
tell, from the interface alone, whether it is talking to the plain core, a
decorated pipeline three layers deep, or a proxy standing in front of
either one.

## Decorator: three layers, one interface, order that matters

Three decorators wrap a `RateSource` in this example, and each one adds
exactly one responsibility. `LoggingRateSource` appends a line to a shared
call log every time `quote()` runs, then delegates unchanged:

```cpp
class LoggingRateSource : public RateSource {
public:
    LoggingRateSource(std::unique_ptr<RateSource> inner, std::vector<std::string>& call_log);
    Cents quote(const Shipment& shipment) override;

private:
    std::unique_ptr<RateSource> inner_;
    std::vector<std::string>& call_log_;
};
```

`CachingRateSource` remembers a quote by shipment key and skips the wrapped
`RateSource` entirely on a repeat lookup for an identical shipment.
`SurchargeRateSource` adds a markup proportional to declared value on top
of whatever the wrapped source quotes, and its rate is mutable through
`set_cents_per_declared_dollar()` so a merchant can adjust it without
rebuilding the pipeline. All three take a `std::unique_ptr<RateSource>` in
their constructor and own it, the same RAII-backed ownership Chapter 8's
`RateEstimator` used for a single strategy; here, each decorator owns
exactly the one layer directly beneath it.

Composing three decorators around a core looks like nested construction,
innermost first:

```cpp
auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
auto caching = std::make_unique<CachingRateSource>(std::move(logging));
SurchargeRateSource pipeline(std::move(caching), /*cents_per_declared_dollar=*/1.5);
```

Reading that from the inside out names the stack: a `TariffTableRateSource`
wrapped in a `LoggingRateSource`, wrapped in a `CachingRateSource`, wrapped
in a `SurchargeRateSource`. `pipeline.quote(shipment)` is the only call a
checkout flow makes, and it cannot tell from that call alone that three
layers sit underneath it rather than zero.

## Order is part of the design, not an implementation detail

The stack above puts caching inside the surcharge: `Surcharge` wraps
`Caching`, which wraps `Logging`, which wraps the core. That ordering has a
real consequence. A cache hit inside `CachingRateSource` returns a
memoized base price without ever calling the `LoggingRateSource` beneath
it, so a repeat lookup for the same shipment produces no new log line. The
surcharge, sitting *outside* the cache, still runs on every single call,
because `SurchargeRateSource::quote()` always calls its wrapped
`RateSource` and adds a fresh markup on top of whatever comes back. Raise
`cents_per_declared_dollar` between two calls for the identical shipment
and the second call's total changes immediately, even though the base rate
underneath it only computed once.

Swap the two decorators' positions, caching outside the surcharge instead
of inside it, and the same rate change behaves differently. Now the cache
memoizes the already-surcharged total, not the base price, so a later
change to the surcharge rate has no effect on a repeat lookup until the
shipment's cache key itself changes. Nothing else in the stack moved. The
two decorators involved are the identical classes, constructed with the
identical arguments; only the order they wrap in changed, and that order
alone decided whether a live rate change shows up on the next quote
or goes unnoticed until a different shipment comes through. This book's
`tests/rate_decorators_test.cpp` asserts both outcomes side by side from
one shipment, confirming the order dependency is a property of the design
and not an artifact of one particular test run.

This is the fact about Decorator worth carrying forward past this
chapter's specific rate pipeline: stacking order changes *what each layer
sees*, not merely what order some log lines print in. A layer only
observes what happens on the side of the stack closer to the core than it
is; anything further out is invisible to it. Deciding where a decorator
goes in a stack is deciding what information reaches it, which is a design
decision with observable consequences, not a stylistic choice a reviewer
can wave past.

## Proxy: the same interface, a different intent

`LazyManifestStoreProxy` implements `ManifestStore` without owning an
`ArchivedManifestStore` up front:

```cpp
class LazyManifestStoreProxy : public ManifestStore {
public:
    std::string lookup(const std::string& tracking_number) override;
    bool is_loaded() const { return static_cast<bool>(real_); }

private:
    std::unique_ptr<ArchivedManifestStore> real_;
};
```

Its `lookup()` constructs the real archive on the first call it ever
receives, then forwards every call after that to the same instance. A
warehouse app holding a `ManifestStore&` cannot tell the difference between
this proxy and a real archive that happened to load instantly, which is
the entire point: the proxy's one job is to make the expensive
construction invisible to the caller, deferring it until a lookup demands
it rather than paying for it at startup whether a lookup ever happens or
not.

`AccessControlledManifestProxy` solves a different problem behind the same
interface. It wraps any `ManifestStore`, including a
`LazyManifestStoreProxy`, proxies composing exactly the way decorators do,
and only forwards a lookup when the caller's clearance meets a required
level:

```cpp
std::string AccessControlledManifestProxy::lookup(const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        throw std::runtime_error("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}
```

This proxy does not reformat a manifest, add a field to it, or change what
a successful lookup returns in any way. A caller with sufficient clearance
gets back exactly what the wrapped
`ManifestStore` would have returned on its own. That is the line between
this chapter's two patterns, drawn as precisely as the forces allow: a
Decorator adds new behavior to what a call returns, and is designed to be
stacked, several of them, in whatever order a caller assembles. A Proxy
controls whether a call reaches its subject at all, construction deferred
or access denied, and is designed to be transparent, leaving a successful
call's result untouched. `CachingRateSource` from the Decorator half of
this chapter looks deceptively close to a caching Proxy by name alone, but
by that same test it is a Decorator: it changes what a caller experiences,
specifically how fast a repeat call returns and whether the call reaches
the layer underneath, in a way meant to compose with other layers, not to
gate a single access decision.

> **AI sidebar.** Ask an LLM pairing partner to "add caching" to an object
> behind an interface, and a common result either wraps the object in a
> new class implementing that same interface, this chapter's Decorator
> shape, or adds a `cache_` member and an `if (cached) return cached;`
> branch directly inside the object's own method body. Both compile and
> both can pass a quick test. The difference shows up the moment a second
> concern needs to be layered on, a surcharge, a logging line, an access
> check: the wrapped version composes by constructing one more layer with
> no change to any existing class, while the branch-inside-the-method
> version means editing that one method again, and again, every time a new
> concern needs to sit somewhere in the sequence. Check which shape a
> generated change took before accepting it. A `cache_` field growing
> inside `TariffTableRateSource` itself is the sign the model solved
> today's request without leaving room for tomorrow's.

## A typed access check: C++20 versus C++23

`AccessControlledManifestProxy::lookup()` above throws
`std::runtime_error` when a caller's clearance falls short, which works but
forces every caller either to wrap the lookup in a `try` block or accept
that an unhandled exception can propagate out of a warehouse scanning
call. C++23's `std::expected` gives the same check a typed alternative: a
return type that carries either the manifest or a specific reason access
was denied, checkable with an `if` and no exception machinery at all.
`try_lookup()` in this example is guarded behind
`#if __cplusplus >= 202302L` precisely so the mandatory C++20 build path
never needs `std::expected`, while the C++23 build gets the more precise
contract.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on denied access works, but it forces every caller to
// wrap the lookup in a try block or accept an unhandled exception on a
// warehouse scanning call.
std::string AccessControlledManifestProxy::lookup(const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        throw std::runtime_error("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}
```
```cpp
// C++23: std::expected reports the same denial as a typed error,
// checkable with an if and no exception machinery.
std::expected<std::string, std::string> AccessControlledManifestProxy::try_lookup(
    const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        return std::unexpected<std::string>("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}
```

Both versions enforce the identical clearance rule against the identical
`AccessLevel`. Only how a caller learns about a denied lookup changes
between the two columns, the same narrow scope this book's codetabs
convention has held to since Chapter 8's typed Strategy error and Chapter
12's typed factory lookup.

## When not to use Decorator or Proxy

Both patterns earn their wrapper when there is a real reason to keep a
responsibility separate from the object it attaches to, or a real reason a
caller must not reach a subject directly. Outside that case, each one adds
an interface implementation with nothing behind it to justify the cost.

A `RateSource` with exactly one concern, no surcharge, no cache, no log,
does not need to be wrapped at all. Calling `TariffTableRateSource::quote()`
directly from checkout code is the plainer non-pattern, and it stays
correct for as long as nothing needs to vary independently around that one
call. The moment to introduce `SurchargeRateSource` is the moment the
surcharge rule itself needs to change on its own schedule, separate from
the tariff table, not before. Wrapping a single-concern object in a
decorator that nothing else ever stacks alongside it adds one more
`quote()` override, one more heap allocation, and one more hop a debugger
has to step through, for a flexibility nobody is using yet.

Proxy fails the same test in a narrower way. An object that is cheap to
construct and reachable by every caller with no access distinction between
them does not need a proxy standing in front of it; it needs to be
constructed directly where it is used. Introducing
`LazyManifestStoreProxy` ahead of any evidence that `ArchivedManifestStore`
construction is expensive enough to matter defends against a cost
that has not been measured, and introducing `AccessControlledManifestProxy`
ahead of a real second caller with lesser clearance defends against a
distinction that does not exist in the system yet. The sharper trap is
building a caching Proxy when what the problem calls for is a
caching Decorator, the distinction this chapter drew directly: a cache
that is meant to compose with other concerns on the same object belongs in
a stack a caller assembles, while a cache whose entire job is gating
whether an expensive call happens at all, with no other concern sharing
that same wrapped object, is closer to what Proxy's intent describes. Name
which force is in play, composing behavior or controlling access,
before reaching for either pattern's shape.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/16-decorator-proxy` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset, 12
under `cpp20`, the difference being the two `std::expected`-specific cases
that only compile under C++23. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the decorator stack and both proxies produce real output rather
than just reading an assertion that says they work, run the example's own
demo script:

```sh
examples/16-decorator-proxy/demo.sh cpp23
```

It prices the same 2 kg, Regional-zone shipment through the decorator
stack twice, once with the surcharge outside the cache and once with the
two swapped, raising the surcharge rate between calls both times so the
difference in behavior is visible in the printed totals, then exercises
both proxies against the same manifest archive:

```
== Decorator: surcharge wraps caching wraps logging wraps core ==
First quote (rate 1.5 cents/declared dollar): $10.90
Second quote, rate raised to 3.0 (base still cached): $12.40
Cache misses: 1, cache hits: 1, log lines: 1
The surcharge recomputes fresh on every call, so raising the rate changes the
total even though the base rate underneath it only computed once.

== Decorator: reordering it to caching wraps surcharge wraps logging wraps core ==
First quote (rate 1.5 cents/declared dollar): $10.90
Second quote, rate raised to 3.0 (total now cached): $10.90
Cache misses: 1, cache hits: 1, log lines: 1
Caching now sits outside the surcharge, so the already-surcharged total is
what gets memoized; raising the rate has no effect until the shipment key changes.

== Proxy: lazy initialization defers the expensive manifest archive ==
Archive loaded before first lookup? no
Lookup TRK-1001: FASTSHIP-MANIFEST acct=acct-fastship-77 dest=CA
Archive loaded after first lookup?  yes
Lookup TRK-2002: SF-MANIFEST account:acct-summit-42 to:CA

== Proxy: access control gates the same interface, stacked on the lazy proxy ==
Guest lookup rejected: access denied: manifest lookup requires warehouse clearance
Warehouse lookup: FASTSHIP-MANIFEST acct=acct-fastship-77 dest=CA

== C++23 refinement: typed access-denial error via std::expected ==
Rejected guest lookup: access denied: manifest lookup requires warehouse clearance
```

The two totals in the first block, $10.90 then $12.40, show the surcharge
rate change took effect on the next call even though "Cache misses: 1,
cache hits: 1" shows the base tariff lookup itself only ran once. The
second block prices the identical shipment through the identical two
decorators, reordered, and the second total stays $10.90, unchanged by the
same rate increase, because caching now sits outside the surcharge instead
of inside it. That contrast is the order-dependency this chapter described
showing up as two different dollar amounts at the terminal, not just as a
passing assertion. Running `examples/16-decorator-proxy/demo.sh cpp20`
prints the same output minus the final `std::expected` block, which only
compiles under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design
  Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley):
  the original Decorator and Proxy entries, including the "attach
  additional responsibilities" and "control access to an object" intent
  statements this chapter's rate pipeline and manifest archive each build
  on.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind
  `examples/16-decorator-proxy`'s suite, which asserts that reordering two
  decorators changes an observable result rather than only checking that
  each class constructs without error.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
