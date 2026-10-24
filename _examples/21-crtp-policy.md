---
layout: example
order: 21
title: "CRTP and policy-based design"
description: Static polymorphism for a rate model and a policy-composed shipping calculator, proven compile-time with static_assert.
github_path: examples/21-crtp-policy
card_eyebrow: "Chapter 21 · CRTP / Policies"
card_title: "Rate model: CRTP + policies"
card_blurb: "A CRTP base dispatches to derived rate models with no vtable, a reusable comparable mixin works across unrelated types, and a calculator is composed from interchangeable compile-time policies."
---

Two static-polymorphism idioms on a shipping-rate domain:

- **CRTP.** `RateModel<Derived>` dispatches `quote()` to the derived
  `compute()` by `static_cast`, with no virtual overhead. A separate
  `Comparable<Derived>` mixin adds ordering to two unrelated types to show the
  self-type trick is general-purpose.
- **Policy-based design.** `ShippingCalculator<Rounding, Surcharge>` composes two
  concept-constrained policy axes; three policy combinations produce three
  correct results with no runtime indirection.

`static_assert`s prove the dispatch resolves at compile time. The C++23 codetabs
path rewrites the CRTP base with `deducing this`.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 13/13 tests pass
./examples/21-crtp-policy/demo.sh cpp23
```

The demo quotes through the CRTP models, orders two unrelated comparable types,
and runs three policy combinations. Chapter 21 covers when static polymorphism
beats virtual dispatch. Builds standalone and under the `cpp20` preset (11
tests, with the `deducing this` cases compiled out).
