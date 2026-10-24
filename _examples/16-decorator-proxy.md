---
layout: example
order: 16
title: "Decorator and Proxy"
description: Stackable rate decorators and access-controlling and lazy proxies over the same interfaces they wrap.
github_path: examples/16-decorator-proxy
card_eyebrow: "Chapter 16 · Decorator / Proxy"
card_title: "Rate decorators and manifest proxies"
card_blurb: "A core rate source wrapped in stackable, order-sensitive decorators (logging, caching, surcharge), next to lazy-init and access-controlling proxies over a manifest store."
---

Two same-interface wrapper patterns with different intent:

- **Decorator.** A `RateSource` core wrapped in stackable layers
  (`LoggingRateSource`, `CachingRateSource`, `SurchargeRateSource`). Order
  matters: `Surcharge(Caching(Logging(Core)))` recomputes the surcharge every
  call while the base stays cached, so a rate change takes effect immediately;
  `Caching(Surcharge(...))` caches the already-surcharged total, so the same
  change has no effect until the key changes. Both are tested.
- **Proxy.** A `ManifestStore` fronted by a `LazyManifestStoreProxy` (defers
  building the real subject until first use) and an
  `AccessControlledManifestProxy` (gates lookups by access level).

A C++23 `try_lookup()` returning `std::expected` is guarded for the C++20 build.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/16-decorator-proxy/demo.sh cpp23
```

The demo quotes through two decorator orders to show the difference, then
exercises the lazy and access-controlled proxies. Chapter 16 distinguishes
Decorator from Proxy (same interface, different intent). Builds standalone and
under the `cpp20` preset (12 tests).
