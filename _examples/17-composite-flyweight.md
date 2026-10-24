---
layout: example
order: 17
title: "Composite, Flyweight, and Null Object"
description: A palletizing model with a recursive package tree, a shared SKU-descriptor pool, and a do-nothing exception handler.
github_path: examples/17-composite-flyweight
card_eyebrow: "Chapter 17 · Composite / Flyweight"
card_title: "Palletizing: composite + flyweight"
card_blurb: "A package tree where leaves and composites share one interface, a flyweight pool that shares SKU descriptors across many line items, and a Null Object that removes null checks."
---

A palletizing model that combines three structural ideas:

- **Composite.** A `PackageComponent` interface with an `Item` leaf and a
  `PackageGroup` composite (one class used as both carton and pallet), so
  `total_weight()` and `item_count()` recurse uniformly over a mixed,
  multi-level tree.
- **Flyweight.** A `SkuCatalog` pool hands out shared
  `shared_ptr<const SkuDescriptor>` flyweights; many line items reference a few
  descriptors, and `pool_size()` proves growth stays bounded.
- **Null Object.** A `NullShipmentExceptionHandler` lets client code call a
  handler unconditionally instead of branching on null.

The C++23 codetabs path uses `std::flat_map` for the pool (vs `std::map` in
C++20), plus a guarded `try_get()` returning `std::expected`.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 18/18 tests pass
./examples/17-composite-flyweight/demo.sh cpp23
```

The demo totals a nested pallet, shows the flyweight pool staying small as line
items grow, and routes an exception through the null and logging handlers.
Chapter 17 covers when each pattern fits. Builds standalone and under the
`cpp20` preset (16 tests).
