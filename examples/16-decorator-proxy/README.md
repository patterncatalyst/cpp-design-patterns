# 16 - Decorator and Proxy: a rate decorator stack and a manifest proxy pair

Both halves of this example wrap an object behind the exact interface that
object already implements. Decorator stacks several wrappers around a
`RateSource` to add behavior, layer by layer, and composes them in any
order. Proxy stands a single wrapper in front of a `ManifestStore` to
control access to it, without adding a new capability to what a lookup
returns.

## Decorator: `src/rate_decorators.hpp`

- `RateSource`: the shared interface. `quote(const Shipment&)` is its only
  method.
- `TariffTableRateSource`: the core component, a plain per-zone tariff
  lookup with no decoration.
- `LoggingRateSource`: appends one line to a shared call log per `quote()`
  call, then delegates unchanged.
- `CachingRateSource`: memoizes a quote by shipment key and skips the
  wrapped `RateSource` entirely on a repeat lookup.
- `SurchargeRateSource`: adds a surcharge proportional to declared value on
  top of whatever the wrapped source quotes; its rate is mutable through
  `set_cents_per_declared_dollar()`.

The order these wrap in is observable, not cosmetic. Stack
`Surcharge(Caching(Logging(Core)))` and the surcharge recomputes fresh on
every call even though the base rate underneath it is cached, so raising
the surcharge rate changes the next quote. Reorder it to
`Caching(Surcharge(Logging(Core)))` and the cache now memoizes the
already-surcharged total, so the same rate change has no effect until the
shipment key itself changes. `tests/rate_decorators_test.cpp` asserts both
outcomes from the identical shipment, differing only in which two
decorators are swapped.

## Proxy: `src/manifest_proxy.hpp`

- `ManifestStore`: the shared interface. `lookup(const std::string&)` is
  its only method.
- `ArchivedManifestStore`: the real subject, standing in for an expensive
  load of a day's manifests off cold storage.
- `LazyManifestStoreProxy`: defers constructing an `ArchivedManifestStore`
  until the first `lookup()` call, then forwards every call after that to
  the same instance.
- `AccessControlledManifestProxy`: wraps any `ManifestStore`, including a
  `LazyManifestStoreProxy`, and only forwards a lookup when the caller's
  `AccessLevel` meets the required clearance.

### C++23 refinement (guarded)

`AccessControlledManifestProxy::try_lookup()` returns
`std::expected<std::string, std::string>` instead of throwing, rejecting a
lookup below the required clearance with a typed error. It is compiled
only under C++23 (`#if __cplusplus >= 202302L` in
`src/manifest_proxy.hpp` and `.cpp`), so the mandatory C++20 build path
never needs `std::expected`. The matching gtest cases in
`tests/manifest_proxy_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/16-decorator-proxy
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild, from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 14 gtest cases under `cpp23` (12 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the decorator stack and both proxies produce real output rather
than just reading an assertion that says they work, run the example's own
demo script. From this directory, `./demo.sh` does the same three steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target decor16_demo
./build/cpp23/decor16_demo
```

Expected output:

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

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the access-control section.

## Files

```
examples/16-decorator-proxy/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    shipping_domain.hpp/.cpp   # Shipment, Cents, shared base_tariff_cost
    rate_decorators.hpp/.cpp   # RateSource + Logging/Caching/Surcharge decorators
    manifest_proxy.hpp/.cpp    # ManifestStore + Lazy/AccessControlled proxies
    demo_main.cpp               # exercises both halves
  tests/
    rate_decorators_test.cpp
    manifest_proxy_test.cpp
```
