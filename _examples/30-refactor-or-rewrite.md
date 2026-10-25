---
layout: example
order: 30
title: "Refactor or rewrite"
description: A legacy freight-pricing engine replaced incrementally behind a stable interface with a strangler fig router, proven by a shared characterization suite.
github_path: examples/30-refactor-or-rewrite
card_eyebrow: "Chapter 30 · Refactor or rewrite"
card_title: "Strangle the legacy engine"
card_blurb: "A legacy pricing engine with undocumented rules replaced by a bounded rewrite behind a stable interface. A strangler fig router shifts traffic customer by customer while a shared suite proves old and new agree before cutover."
---

A freight-pricing engine migrated without a big-bang rewrite:

- **Legacy engine.** Carries an undocumented zone-rate matrix, two undocumented
  customer contract overrides, and an unexplained rounding rule, all pinned by a
  shared characterization suite.
- **Bounded rewrite.** A modern engine behind the same `PricingEngine`
  interface, with restructured internals and every piece of embedded knowledge
  preserved, judged against the identical test table.
- **Strangler fig router.** Routes each quote to the legacy engine, the new one,
  or both in compare mode, shifting customer by customer. A comparison test
  catches a stand-in engine built to disagree while still
  returning the safe legacy answer, before any real cutover.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/30-refactor-or-rewrite/demo.sh cpp23
```

The demo plays back a five-release migration in which every customer total stays
identical, so the cutover is invisible. The stable interface exposes a C++23
`try_quote()` returning `std::expected`, guarded for C++20. Builds standalone
and under the `cpp20` preset (12 tests).
