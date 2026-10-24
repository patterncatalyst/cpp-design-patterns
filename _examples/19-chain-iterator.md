---
layout: example
order: 19
title: "Chain of Responsibility and Iterator"
description: An order-approval handler chain and a hand-written forward iterator that also works as a C++20 range.
github_path: examples/19-chain-iterator
card_eyebrow: "Chapter 19 · Chain / Iterator"
card_title: "Approval chain and range traversal"
card_blurb: "A request travels a chain of approval handlers until one acts, and a bucketed manifest exposes a forward iterator that composes directly with views::filter and views::transform."
---

Two traversal/coordination patterns:

- **Chain of Responsibility.** An `ApprovalHandler` chain (weight limit, hazmat,
  embargo, then a manual-approval fallback) where each handler either acts or
  passes the request on. A call log records exactly how far each request
  travelled, so tests assert the path, not just the final decision.
- **Iterator.** `PackageManifest` stores packages in four zone buckets but
  exposes a hand-written forward iterator that flattens them into zone order.
  `static_assert(std::forward_iterator<...>)` and
  `static_assert(std::ranges::forward_range<...>)` prove it is a real range, so
  `std::views::filter` and `std::views::transform` compose over it directly.

The C++23 codetabs path ends the ranges pipeline with `std::ranges::to<std::vector>()`
(vs a manual iterator-pair construction in C++20).

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 9/9 tests pass
./examples/19-chain-iterator/demo.sh cpp23
```

The demo routes several shipments through the approval chain, printing where
each stops, then traverses the manifest with the iterator and with a ranges
pipeline. Chapter 19 covers when each pattern fits. Builds standalone and under
the `cpp20` preset (8 tests).
