# 19 - Chain of Responsibility and Iterator: a shipment-approval chain and a zone-bucketed package manifest

Chain of Responsibility routes a `ShipmentRequest` down a chain of handlers
until one of them acts on it. Iterator provides sequential access to a
`PackageManifest` without exposing that the manifest stores packages
bucketed by destination zone rather than as one flat list, both as a
hand-written forward iterator and as a modern ranges pipeline over the same
data.

## Chain of Responsibility: `src/approval_chain.hpp`

- `ApprovalHandler`: the shared base. `handle()` logs which handler it
  consulted, then either returns `try_handle()`'s decision or delegates to
  the next handler.
- `WeightLimitHandler`, `HazmatHandler`, `EmbargoHandler`: each rejects a
  request that fails its own check, or passes an otherwise-fine request on.
- `ManualApprovalHandler`: the fallback at the end of the chain, with no
  next handler. Its `try_handle()` always returns a decision
  (`NeedsManualReview`), so the chain is guaranteed to terminate.
- `build_default_chain()`: constructs `WeightLimitHandler(40.0) ->
  HazmatHandler({US, CA}) -> EmbargoHandler({KP, IR}) ->
  ManualApprovalHandler`.

`tests/approval_chain_test.cpp` asserts both the final decision and the
full `call_log` for four requests, confirming a request stops at the first
handler that acts rather than merely checking the end result.

## Iterator: `src/package_manifest.hpp`

- `PackageManifest`: stores `Package` values in four buckets, one per
  `DestinationZone` (`Local`, `Regional`, `National`, `International`).
- `PackageManifest::const_iterator`: a hand-written forward iterator
  (`std::forward_iterator_tag`) that flattens the four buckets into one
  zone-ordered traversal. It satisfies `std::forward_iterator`, and
  `PackageManifest` itself satisfies `std::ranges::forward_range`
  (both asserted with `static_assert` in `src/package_manifest.cpp`), so it
  interoperates with `std::find_if`, range-`for`, and a
  `std::views::filter` + `std::views::transform` ranges pipeline with no
  adapter code.

### C++23 refinement (guarded)

`demo_main.cpp` and `tests/package_manifest_test.cpp` add a
`std::ranges::to<std::vector>()` stage to the same filter-then-transform
pipeline, collecting it directly into a `std::vector` instead of
constructing one by hand from the view's iterators. It is compiled only
under C++23 (`#if __cplusplus >= 202302L`), so the mandatory C++20 build
path never needs it.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/19-chain-iterator
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

Either path discovers the same gtest cases via `gtest_discover_tests`: 9
under `cpp23`, 8 under `cpp20` (the one `std::ranges::to` case is compiled
out). Observed locally on Fedora 44 / clang 22.1.8:

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

Expected output:

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

The last block only prints under the `cpp23` preset; under `cpp20` the demo
stops after the ranges filter + transform section.

## Files

```
examples/19-chain-iterator/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    approval_chain.hpp/.cpp    # Chain of Responsibility: handlers + build_default_chain()
    package_manifest.hpp/.cpp  # Iterator: bucketed manifest + forward iterator
    demo_main.cpp                # exercises both halves
  tests/
    approval_chain_test.cpp
    package_manifest_test.cpp
```
