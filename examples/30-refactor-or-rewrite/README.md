# 30 - Refactor or rewrite

A freight carrier's pricing engine has outgrown the team that wrote it: a
zone-rate matrix grown lane by lane, two customer contract discounts
expressed as raw arithmetic, and a round-up-to-the-quarter-dollar step
nobody can trace to a current requirement. This example builds a bounded
rewrite of that engine and a strangler fig seam that migrates traffic to
it one customer at a time, proving at every step that nothing a customer
sees has changed.

## The domain

- `PricingEngine` (`src/pricing_engine.hpp`): the stable interface. Every
  concrete engine, and the router that chooses between them, implements
  `quote()`. Under C++23 the base class also offers `try_quote()`,
  returning `std::expected<Quote, PricingError>`, implemented once with no
  changes to either concrete engine.
- `LegacyPricingEngine` (`src/legacy_pricing_engine.hpp`): the component
  under review. Holds an undocumented zone-rate matrix, two undocumented
  customer overrides (`ACME-2019`'s volume rebate, `GLOBEX`'s flat
  discount), and an unexplained round-up-to-the-quarter-dollar step
  inherited from a decommissioned invoicing system.
- `ModernPricingEngine` (`src/modern_pricing_engine.hpp`): the bounded
  rewrite. Same `PricingEngine` contract, same quoted totals, restructured
  internals: a named `zone_rate_card()`, a named `CustomerAdjustment`
  table instead of an if/else chain, and the quarter-dollar rule kept, not
  deleted, because nothing in the rewrite confirmed it is safe to drop.
- `StranglerPricingRouter` / `CutoverPolicy` (`src/strangler_pricing_router.hpp`):
  the strangler fig seam. Four modes: `LegacyOnly`, `ModernOnly`,
  `Compare` (runs both, logs disagreements, returns legacy's answer),
  and `Routed` (a `CutoverPolicy` decides per customer which engine
  answers).

## What the characterization table proves

`tests/pricing_characterization_cases.hpp` holds seven shipment profiles
and the exact totals `LegacyPricingEngine` returns for each, captured by
running the legacy engine, not derived from a specification. Three test
files are judged against this one table:

- `tests/legacy_pricing_engine_test.cpp` confirms `LegacyPricingEngine`
  still matches every case (the safety net itself).
- `tests/modern_pricing_engine_test.cpp` confirms `ModernPricingEngine`
  matches the identical cases, the proof that the rewrite preserved every
  piece of undocumented knowledge the legacy engine carried.
- `tests/legacy_modern_agreement_test.cpp` compares the two engines
  directly against each other, case by case, independent of either
  per-engine test.

## What the router proves

`tests/strangler_pricing_router_test.cpp` exercises all four modes,
including a `BuggyModernEngine` test double standing in for a modern
engine with a real defect. In `Compare` mode, the router detects the
disagreement and still returns the legacy engine's answer, proving a bug
in the new engine during the verification window cannot reach a customer.
`Routed` mode proves a `CutoverPolicy` sends only migrated customers to
`ModernPricingEngine`, leaving every other customer on legacy until their
turn comes.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/30-refactor-or-rewrite
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

`PricingEngine::try_quote()` only exists under C++23, so the two presets
discover a different test count. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the migration itself, not just the tests that back it, run the
example's own demo script. It plays back five releases of the same
cutover, from a cold `LegacyOnly` rollout to a `ModernOnly` finish:

```sh
examples/30-refactor-or-rewrite/demo.sh cpp23
```

Expected output:

```
== Release 1: LegacyOnly, nothing has moved yet ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75

== Release 2: Compare mode, modern runs silently alongside legacy ==
disagreements found: 0 (0 means modern is safe to start taking traffic)

== Release 3: Routed, ACME-2019 migrated, GLOBEX and WIDGETCO still legacy ==
ACME-2019 quote (now via modern): $16.00
GLOBEX quote (still via legacy): $5.00
customers migrated so far: 1 of 3

== Release 4: Routed, every customer migrated ==
GLOBEX quote (now via modern): $5.00
WIDGETCO quote (now via modern): $68.75
customers migrated so far: 3 of 3

== Release 5: ModernOnly, LegacyPricingEngine can be deleted ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75
```

Every quote is identical across all five releases. That sameness is the
entire point of a strangler fig migration: the cutover from
`LegacyPricingEngine` to `ModernPricingEngine` is invisible to the
customer precisely because the characterization table and the agreement
test proved it would be, before `StranglerPricingRouter` ever routed a
single real customer to the new engine. Running
`examples/30-refactor-or-rewrite/demo.sh cpp20` prints identical output;
nothing in the demo path is standard-dependent.

## Files

```
examples/30-refactor-or-rewrite/
  CMakeLists.txt                 # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json              # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                   # standalone Conan recipe (gtest/1.17.0)
  demo.sh                        # build + run the five-release migration demo
  src/
    pricing_engine.hpp                   # the stable interface (+ C++23 try_quote NVI)
    legacy_pricing_engine.hpp/.cpp       # the component under review
    modern_pricing_engine.hpp/.cpp       # the bounded rewrite
    strangler_pricing_router.hpp/.cpp    # the strangler fig seam + CutoverPolicy
    demo_main.cpp                        # plays back the five-release migration
  tests/
    pricing_characterization_cases.hpp       # the shared oracle both engines are judged against
    legacy_pricing_engine_test.cpp           # pins legacy's current behavior
    modern_pricing_engine_test.cpp           # proves the rewrite matches the same table
    legacy_modern_agreement_test.cpp         # direct engine-to-engine comparison
    strangler_pricing_router_test.cpp        # routing modes + disagreement detection
```
