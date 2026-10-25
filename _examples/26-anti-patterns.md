---
layout: example
order: 26
title: "Anti-patterns and pattern misuse"
description: An over-patterned shipping design and its simplified rewrite, side by side, with tests proving identical behavior and that only the rewrite is testable in isolation.
github_path: examples/26-anti-patterns
card_eyebrow: "Chapter 26 · Anti-patterns"
card_title: "Patternitis, and backing out of it"
card_blurb: "A singleton registry, a factory-of-factories, and a god object, collapsed into one service that takes a settings value by injection. Tests prove the rewrite computes the same answer and is testable where the original was not."
---

A before-and-after on one shipping domain, showing four misuses and the rewrite
that removes them:

- **Singleton as global mutable state.** A configuration registry reachable from
  anywhere, whose surcharge leaks across unrelated calculations.
- **Speculative generality.** A factory-of-factories wrapping interfaces that
  have exactly one implementation each.
- **God object.** A manager that validates, logs, formats, and prices.
- **Leaky abstraction.** A method whose correctness depends on the order you
  called the others.

The rewrite collapses five types into one `ShipmentPricingService` that takes a
`RateSettings` value by injection.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 10/10 tests pass
./examples/26-anti-patterns/demo.sh cpp23
```

One test table proves the over-patterned and simplified designs compute
identical totals and throw identically. A second set proves the contrast that
matters: two independently configured pricing services never affect each other,
while the singleton form leaks state between calculations. Nothing here depends
on a C++23-only feature, so the suite is 10/10 under both the `cpp23` and
`cpp20` presets.
