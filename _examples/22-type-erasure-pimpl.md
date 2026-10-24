---
layout: example
order: 22
title: "Type erasure and PImpl"
description: A value-semantic AnyRateQuoter that erases unrelated concrete quoters, and a PImpl ledger that is a true compilation firewall.
github_path: examples/22-type-erasure-pimpl
card_eyebrow: "Chapter 22 · Type erasure / PImpl"
card_title: "AnyRateQuoter and a PImpl ledger"
card_blurb: "A hand-written concept/model wrapper holds any quoter type by value with no shared base, and a PImpl ledger keeps its implementation behind a pointer so its header never leaks detail."
---

Two idioms that both hide a type behind a stable surface:

- **Type erasure.** `AnyRateQuoter` wraps any type satisfying a `Quotable`
  concept behind a private hand-written `Concept`/`Model` pair
  (`unique_ptr<Concept>` + `clone()` for deep copies). Four unrelated concrete
  quoters share no base class; a `SurgeQuoter` with a per-call counter proves a
  copy is independent, and an `InsuredQuoter` wraps another `AnyRateQuoter`
  (erasure composed, the ch16 Decorator tie without inheritance).
- **PImpl.** `RateQuoteLedger` keeps its implementation behind a
  `unique_ptr<Impl>` so the header exposes nothing: adding fields to `Impl` never
  changes `sizeof(RateQuoteLedger)` or forces consumers to recompile. Goes
  deeper than ch15 on ABI stability and the `unique_ptr` const-propagation
  gotcha.

A C++23 `try_record()` returning `std::expected` is guarded for C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 12/12 tests pass
./examples/22-type-erasure-pimpl/demo.sh cpp23
```

The demo stores four unrelated quoters in one vector, copies a stateful quoter
to show independence, composes an insured wrapper, and records quotes through the
PImpl ledger. Chapter 22 covers when to reach for each idiom. Builds
standalone and under the `cpp20` preset (10 tests).
