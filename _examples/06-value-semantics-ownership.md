---
layout: example
order: 6
title: "Value semantics and ownership"
description: Four ownership shapes in one warehouse-dispatch graph, from Rule-of-Zero value types to unique, shared, and weak pointers.
github_path: examples/06-value-semantics-ownership
card_eyebrow: "Chapter 6 · Ownership"
card_title: "Ownership: value, unique, shared, weak"
card_blurb: "A dispatch graph showing a Rule-of-Zero value type, sole ownership via unique_ptr, shared ownership via shared_ptr, and a weak_ptr that breaks a reference cycle."
---

A warehouse-dispatch object graph that makes the ownership choices concrete:

- **`Parcel`** is a value type following the Rule of Zero (plain members, deep
  copies are independent).
- **`DockSlot`** holds sole ownership via `unique_ptr<Parcel>`;
  `transfer_parcel()` moves a parcel between slots.
- **`CarrierContract`** follows the Rule of Five (it owns a raw tier-rate
  array) and is shared across `ShippingRoute`s via `shared_ptr`, so
  renegotiating through one route is visible to all routes sharing it.
- **`ManifestNode`** owns its children via `shared_ptr` and observes its parent
  via `weak_ptr`, breaking the cycle a bidirectional `shared_ptr` would create.

The C++23 codetabs path adopts a raw pointer from a C-style factory with
`std::out_ptr`; the C++20 path does it by hand. Both compile and test identically.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 22/22 tests pass
./examples/06-value-semantics-ownership/demo.sh cpp23
```

The demo moves a parcel between dock slots, renegotiates a shared carrier
contract and shows every sharing route observe the change, and lets a manifest
parent expire while a child's `weak_ptr` reports it gone. Chapter 6 walks
through when to reach for each ownership kind. Builds standalone and under the
`cpp20` preset (22 tests).
