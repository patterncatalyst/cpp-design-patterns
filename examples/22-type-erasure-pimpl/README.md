# 22 - Type erasure and PImpl: a quoting ledger

A checkout flow collects quotes from quoters that have nothing in common
but one method, and needs a running ledger of what was quoted without
exposing how that ledger is implemented. This example builds both idioms
on the same shipping domain so they compare directly.

## The domain

- `Shipment`: `weight_kg`, a `DestinationZone`, and `declared_value_cents`,
  the same shape every other chapter's example uses.
- Money is tracked as integer `Cents`, never floating point.

## Type erasure: `AnyRateQuoter`

- `Quotable` (`src/any_rate_quoter.hpp`) is a concept requiring only a
  `quote(const Shipment&) -> Cents` member; no shared base class.
- `AnyRateQuoter` holds any `Quotable` type by value, through a private
  `Concept`/`Model<Quoter>` pair, the same concept/model technique
  `std::function` and `std::any` use internally. Copy, move, and
  destruction are value-semantic: a copy deep-clones through
  `Concept::clone()`.
- `src/rate_quoters.hpp` defines four concrete quoters sharing no base
  class: `FlatRateQuoter`, `WeightBasedQuoter`, `SurgeQuoter` (mutable
  per-call state, used to prove copies are independent), and
  `InsuredQuoter` (wraps another `AnyRateQuoter` and adds a surcharge, the
  same composition Chapter 8's `with_insurance_surcharge()` built with
  `std::function`).

## PImpl: `RateQuoteLedger`

- `src/rate_quote_ledger.hpp` declares the public operations only;
  `src/rate_quote_ledger.cpp` defines `Impl` and every bookkeeping detail.
- Hand-written out-of-line destructor, deep-clone copy constructor and
  copy assignment, and defaulted move constructor and move assignment, the
  same shape Chapter 15's `LabelRenderer` introduced. See the chapter for
  why each of those five special members looks the way it does.
- `record()` throws `std::invalid_argument` on a negative amount;
  C++23's `try_record()` returns `std::expected<void, std::string>`
  instead, guarded behind `#if __cplusplus >= 202302L`.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/22-type-erasure-pimpl
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

To see type erasure and PImpl both producing real output, build and run
the demo binary. From this directory, `./demo.sh` does the install/build
steps above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target erase22_demo
./build/cpp23/erase22_demo
```

## Files

```
examples/22-type-erasure-pimpl/
  CMakeLists.txt          # standalone + superbuild-integrated targets
  CMakePresets.json        # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py              # standalone Conan recipe (gtest/1.17.0)
  demo.sh                   # build + run the demo binary
  src/
    shipping_domain.hpp            # Shipment, Cents, DestinationZone
    any_rate_quoter.hpp            # Quotable concept + AnyRateQuoter
    rate_quoters.hpp               # four unrelated concrete quoters
    rate_quote_ledger.hpp/.cpp     # RateQuoteLedger PImpl firewall
    demo_main.cpp                  # exercises both idioms
  tests/
    any_rate_quoter_test.cpp
    rate_quote_ledger_test.cpp
```
