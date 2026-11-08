---
title: "Decorator and Proxy"
order: 16
part: "composing-adapting-structure"
description: "Two structural patterns that wrap an object behind its own interface, taught against a carrier rate pipeline and a manifest archive: Decorator stacks layers that each add one responsibility in an order that changes the result, and Proxy stands a single surrogate in front of an expensive or access-controlled subject without adding any new capability."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Wrap an object behind the interface it already implements, so a caller can't tell how many wrappers sit in front of the real thing.
- Build a Decorator stack where each layer adds one responsibility, and predict how the wrapping order changes the result.
- Build a lazy-initialization Proxy that defers an expensive subject until the first call, and an access-control Proxy that gates a call on a policy check.
- Tell Decorator and Proxy apart by intent: one adds behavior and stacks, the other controls access and stays transparent.
- Choose between throwing on a denied access check and returning `std::expected`, new in C++23.
- Recognize when neither wrapper has anything to do, and what to write instead.

## Why this matters

A checkout pipeline that asks for a shipping quote wants more than the quote. It wants a surcharge on high-value shipments, a cache so a cart priced three times doesn't hit the tariff table three times, and a log line for the next billing dispute. Each concern ships on its own schedule: the surcharge changes when finance renegotiates, the cache arrives after load testing, the log after support asks for an audit trail.

A warehouse scanning app has a different problem with the same shape. The real manifest archive is expensive to open, and a guest account mustn't read a manifest that a warehouse badge unlocks. Neither concern is the archive's own logic: one is *when* the archive is built, the other is *whether* a caller may reach it.

Both patterns wrap an object behind its own interface, and their code can look nearly identical. What separates them is what each one is for.

{% include excalidraw.html name="16-decorator-proxy" caption="Figure 16.1: A Decorator stack (surcharge, caching, logging) wrapping a core RateSource, next to a Proxy (AccessControlledManifestProxy) controlling access to a real ManifestStore subject. Both wrap the same interface their subject implements; only the intent differs." %}

## Pattern: Decorator

### The problem

Surcharge, caching and logging all happen *around* a quote. Putting them inside `TariffTableRateSource::quote()` means editing that method each time a concern is added or reordered, and every caller gets every concern whether it wants it or not.

### The solution

**A Decorator attaches one responsibility to an object by wrapping it in another object with the same interface, so wrappers stack in any order a caller assembles.**

### How C++ models it

Every layer implements `RateSource`:

```cpp
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) = 0;
};
```

`TariffTableRateSource` is the core, a per-zone, per-kilogram lookup. Each decorator takes a `std::unique_ptr<RateSource>` and owns the one layer directly beneath it. `LoggingRateSource` appends a line to a shared call log, then delegates unchanged:

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

`CachingRateSource` remembers a quote by a key built from weight, zone and declared value, and skips the wrapped source on a repeat. `SurchargeRateSource` adds a markup proportional to declared value, and `set_cents_per_declared_dollar()` changes the rate without rebuilding the pipeline.

A stack is nested construction, innermost first:

```cpp
auto logging = std::make_unique<LoggingRateSource>(std::make_unique<TariffTableRateSource>(), call_log);
auto caching = std::make_unique<CachingRateSource>(std::move(logging));
SurchargeRateSource pipeline(std::move(caching), /*cents_per_declared_dollar=*/1.5);
```

Checkout calls only `pipeline.quote(shipment)` and can't tell three layers sit beneath it.

### Order changes the result

A layer sees only what happens between it and the core. In the stack above, a cache hit returns the memoized base price without calling `LoggingRateSource`, so a repeat produces no new log line. The surcharge sits outside the cache and runs on every call, so raising its rate changes the next total even though the base rate computed once.

Swap caching and surcharge and the cache memoizes the surcharged total. The same rate change now has no effect on a repeat lookup until the shipment's cache key changes. The two classes and their arguments are identical; only the order moved.

{% include excalidraw.html name="16-decorator-ordering" caption="Figure 16.2: the same two decorators, reordered. Moving Cache outside Surcharge changes which total a repeat call returns, because a cache hit now short-circuits before Surcharge ever runs again." %}

For a 2 kg Regional shipment, raising the surcharge from 1.5 to 3.0 cents per declared dollar between two calls:

| Stack, outermost first | First quote | Second quote | Log lines |
| --- | --- | --- | --- |
| Surcharge, Caching, Logging, core | $10.90 | $12.40 | 1 |
| Caching, Surcharge, Logging, core | $10.90 | $10.90 | 1 |

Choosing where a decorator goes is choosing what information reaches it.

## Pattern: Proxy

### The problem

`ArchivedManifestStore` stands in for a day's manifests loaded off cold storage before the first tracking number resolves. Building it at startup pays that cost whether a lookup ever happens. And some callers must not reach it at all.

### The solution

**A Proxy is a surrogate with the subject's interface that controls when or whether a call reaches the subject, leaving a successful call's result unchanged.**

### How C++ models it

The subject's interface:

```cpp
class ManifestStore {
public:
    virtual ~ManifestStore() = default;
    virtual std::string lookup(const std::string& tracking_number) = 0;
};
```

`LazyManifestStoreProxy` implements it without owning an archive up front:

```cpp
class LazyManifestStoreProxy : public ManifestStore {
public:
    std::string lookup(const std::string& tracking_number) override;
    bool is_loaded() const { return static_cast<bool>(real_); }

private:
    std::unique_ptr<ArchivedManifestStore> real_;
};
```

The first `lookup()` constructs the archive, and every later call forwards to the same instance. A caller holding a `ManifestStore&` sees an archive that happened to load instantly.

`AccessControlledManifestProxy` wraps any `ManifestStore`, including the lazy proxy, and forwards only when the caller's clearance is high enough:

```cpp
std::string AccessControlledManifestProxy::lookup(const std::string& tracking_number) {
    if (caller_level_ < kRequiredLevel) {
        throw std::runtime_error("access denied: manifest lookup requires warehouse clearance");
    }
    return real_->lookup(tracking_number);
}
```

A caller with clearance gets back exactly what the wrapped store returns.

### Decorator or Proxy

| | Decorator | Proxy |
| --- | --- | --- |
| Changes | What the call does or returns | When or whether the call reaches the subject |
| Designed to | Stack, several in a caller-chosen order | Stand alone and stay transparent |
| Examples here | `LoggingRateSource`, `CachingRateSource`, `SurchargeRateSource` | `LazyManifestStoreProxy`, `AccessControlledManifestProxy` |

`CachingRateSource` sounds like a caching Proxy, but by this measure it's a Decorator: it changes how fast a repeat returns and whether the call reaches the layer beneath, and it's built to compose with other layers. A cache whose only job is deciding whether one expensive call happens, with no other concern on that object, is closer to Proxy.

### A typed access check

The throwing `lookup()` forces every caller to wrap the call in `try` or let the exception leave the scanning call. `std::expected` returns either the manifest or the reason access was denied. `try_lookup()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never sees it.

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

Both enforce the same clearance rule. Only how a caller learns about a denial changes.

## When not to use it

- **Decorator.** A `RateSource` with one concern doesn't need wrapping; call `TariffTableRateSource::quote()` directly. Add `SurchargeRateSource` when the surcharge rule must change on its own schedule. A decorator that nothing stacks beside costs an override, a heap allocation and a debugger hop for flexibility nobody uses.
- **Proxy.** An object that is cheap to construct and open to every caller needs no surrogate. Add a lazy proxy after measuring that construction matters, and an access proxy when a second caller with lesser clearance exists.
- **A cache.** Decide which force is in play before choosing the shape. A cache that composes with other concerns belongs in a decorator stack; a cache that only gates one expensive call is a proxy.

> **🤖 Explore with your agent**
>
> "Add `CachingRateSource` as a decorator implementing `RateSource` that owns its inner source by `std::unique_ptr`, keyed on weight, zone and declared value. Leave `TariffTableRateSource` unchanged. Place the cache inside `SurchargeRateSource`, so a surcharge rate change applies to the next quote."
>
> Run `ctest --preset cpp23 -R 'CachingRateSource|DecoratorStack'`, and check that `TariffTableRateSource` gained no `cache_` member.

## Build, run, observe

From the repository root, the superbuild picks up `examples/16-decorator-proxy`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 cases pass under `cpp23` and 12 under `cpp20`; the two `std::expected` cases compile only under C++23. To see the stacks and proxies run:

```sh
examples/16-decorator-proxy/demo.sh cpp23
```

It prices one 2 kg Regional shipment through both orderings, raising the surcharge between calls, then runs both proxies against the same archive:

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

The archive stays unloaded until the first lookup, and the guest is refused through the same interface the warehouse caller uses. Under `cpp20` the output stops before the `std::expected` block.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Decorator and Proxy entries, including the "attach additional responsibilities" and "control access to an object" intent statements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/16-decorator-proxy`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
