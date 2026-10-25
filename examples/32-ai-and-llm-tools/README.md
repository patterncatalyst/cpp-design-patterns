# 32 - Working with AI and LLM tools

A metered API gateway classifies a customer's monthly request count into
one of three billing tiers: Starter, Growth, Scale. The classification
rule varies by exactly one input, so an engineer names the force and asks
an AI pairing partner for a Strategy. This example builds the result two
ways: a correct implementation matching the pricing spec, and an
AI-plausible implementation with one comparison operator wrong at the
Starter/Growth boundary. A behavior table catches the difference.

## The domain

- `UsageTier` / `ToString` (`src/usage_tier.hpp`): the three billing
  tiers and their display names.
- `UsageTierStrategy` (`src/usage_tier_strategy.hpp`): the interface,
  one virtual method, `Classify(long monthly_requests)`.
- `CorrectUsageTierStrategy` (`src/correct_usage_tier_strategy.hpp`):
  the pricing spec's actual boundaries. Starter below 10,000
  requests/month, Growth from 10,000 up to 99,999, Scale from 100,000 up.
  Both lower bounds are inclusive.
- `PlausibleWrongUsageTierStrategy`
  (`src/plausible_wrong_usage_tier_strategy.hpp`): the shape an AI
  pairing partner handed back for the identical prompt. Same interface,
  same three tiers, same structure. One known bug: `<=` instead of `<`
  on the Starter/Growth boundary, so exactly 10,000 requests stays
  classified as Starter.
- `MonthlyBillUsd` (`src/billing.hpp`): turns a tier into a dollar
  figure. Starter is metered per request with no bundled allotment;
  Growth and Scale are flat monthly fees. This is what turns the
  boundary bug into a concrete billing error rather than an abstract
  correctness issue.

## What the behavior table proves

`tests/usage_tier_classification_test.cpp` defines one shared function,
`ExpectSpecCompliantClassification`, asserting the full boundary table
(every tier's low end, middle, and high end) against whichever strategy
it is given:

- Run against `CorrectUsageTierStrategy`, every assertion passes. This is
  the test a continuous integration pipeline gates a merge on.
- Run against `PlausibleWrongUsageTierStrategy`, the 10,000-request case
  alone disagrees with the spec. The test proves this directly, wrapping
  the call in GoogleTest's `EXPECT_NONFATAL_FAILURE`, which requires the
  wrapped statement to produce exactly one failure and checks that the
  failure message mentions the boundary value. The overall suite stays
  green; the demonstration of the catch is itself a passing assertion.
- A third test, `BothStrategiesAgreeEverywhereAwayFromTheBoundary`, shows
  how easy the bug is to miss on a casual read: every input except the
  exact boundary produces identical results from both strategies.

`tests/billing_test.cpp` turns the same bug into a dollar figure: at
exactly 10,000 requests, the correct classification bills a flat $99
Growth rate, and the plausible-wrong classification keeps the customer on
Starter's $0.02-per-request metering, a $101 overcharge traceable to one
comparison operator.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/32-ai-and-llm-tools
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

Nothing in this chapter's code depends on a C++23-only feature, so both
presets discover the identical set of gtest cases via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 6   # cpp23
100% tests passed, 0 tests failed out of 6   # cpp20
```

To see the correct and plausible-wrong strategies classify the same
requests side by side, and the dollar amount the boundary bug costs, run
the example's own demo script:

```sh
examples/32-ai-and-llm-tools/demo.sh cpp23
```

Expected output:

```
== CorrectUsageTierStrategy: lower tier boundary is inclusive ==
      9999 requests -> Starter ($199.98/mo)
     10000 requests -> Growth  ($99.00/mo)
     50000 requests -> Growth  ($99.00/mo)
     99999 requests -> Growth  ($99.00/mo)
    100000 requests -> Scale   ($499.00/mo)

== PlausibleWrongUsageTierStrategy: AI-drafted, looks identical on read ==
      9999 requests -> Starter ($199.98/mo)
     10000 requests -> Starter ($200.00/mo)
     50000 requests -> Growth  ($99.00/mo)
     99999 requests -> Growth  ($99.00/mo)
    100000 requests -> Scale   ($499.00/mo)

== The boundary a read-only review missed ==
At exactly 10,000 requests:
  correct classification: Growth ($99.00/mo)
  plausible-wrong classification: Starter ($200.00/mo)
  billing error from one comparison operator: $101.00
```

Running `examples/32-ai-and-llm-tools/demo.sh cpp20` prints identical
output; nothing here is standard-dependent.

## Files

```
examples/32-ai-and-llm-tools/
  CMakeLists.txt             # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json          # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py               # standalone Conan recipe (gtest/1.17.0)
  demo.sh                    # build + run the demo binary
  src/
    usage_tier.hpp/.cpp                         # UsageTier enum + ToString
    usage_tier_strategy.hpp                     # the Strategy interface
    correct_usage_tier_strategy.hpp/.cpp        # correct: inclusive lower bounds
    plausible_wrong_usage_tier_strategy.hpp/.cpp  # AI-plausible, one off-by-one
    billing.hpp/.cpp                            # tier -> dollar figure
    demo_main.cpp                               # exercises both strategies side by side
  tests/
    usage_tier_classification_test.cpp   # the verification gate: catches the bug
    billing_test.cpp                     # the dollar-figure consequence
```
