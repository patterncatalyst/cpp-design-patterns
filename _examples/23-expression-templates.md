---
layout: example
order: 23
title: "Expression templates and generic idioms"
description: A lazy route-metrics expression that evaluates once, plus a self-registering policy registry and a templated serializer.
github_path: examples/23-expression-templates
card_eyebrow: "Chapter 23 · Expression templates"
card_title: "Route metrics: lazy evaluation"
card_blurb: "An expression template that defers a weighted sum into a tree evaluated in one pass, proven against eager evaluation, plus a self-registering registry and a field-hook templated serializer."
---

Three generic idioms on a freight route-reporting domain:

- **Expression templates (lead).** `0.5*leg_one + 0.3*leg_two + 0.2*leg_three`
  defers into a five-node lazy tree; only the final conversion builds a real
  `RouteMetrics`. A construction counter proves the lazy path builds **1** object
  where the eager path builds **5**, with bit-identical results.
- **Self-registering prototype registry.** `CarrierPolicyRegistrar<Concrete>`
  lets three policies register themselves from their own translation units (the
  core is an OBJECT library so the linker cannot drop the registrations).
- **Templated serialization.** One `describe_fields()` hook drives both
  `serialize()` and `deserialize()` via `std::apply` and fold expressions.

A C++23 `try_create()` returning `std::expected` is guarded for C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 11/11 tests pass
./examples/23-expression-templates/demo.sh cpp23
```

The demo evaluates the lazy and eager route sums side by side with their
construction counts, resolves self-registered policies, and round-trips a carrier
profile through the templated serializer. Chapter 23 covers when this machinery
is worth its weight. Builds standalone and under the `cpp20` preset (9 tests).
