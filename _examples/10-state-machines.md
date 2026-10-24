---
layout: example
order: 10
title: "State and finite state machines"
description: A shipment lifecycle expressed four ways, including a deterministic stochastic machine.
github_path: examples/10-state-machines
card_eyebrow: "Chapter 10 · State / FSM"
card_title: "Shipment lifecycle: four machines"
card_blurb: "The same shipment state rules as a GoF State pattern, a table-driven FSM, a std::variant FSM, and a probability-driven machine, with cross-check tests proving they agree."
---

A shipment lifecycle (`Created → Paid → InTransit → Delivered`, with
`Cancelled` and a stochastic-only `Lost`) expressed four independent ways that
share only the status and event types, so cross-check tests prove they agree
rather than assume it:

- **GoF State pattern** — one type per status, default-reject base methods,
  `unique_ptr`-owned context.
- **Table-driven FSM** — a `(status, event) → status` map.
- **`std::variant` FSM** — tag types plus `std::visit` over an overload set.
- **Probability-driven FSM** — deterministic events plus a probabilistic
  `advance()` using `std::discrete_distribution` over configured weights.

The stochastic machine takes its `std::mt19937` by constructor injection, so
tests seed it with a fixed value and assert an exact reproducible sequence,
while a 20,000-trial test checks the empirical frequencies match the weights.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 31/31 tests pass
./examples/10-state-machines/demo.sh cpp23
```

The demo drives a shipment through legal transitions, rejects an illegal one,
and runs a seeded stochastic sequence. Chapter 10 walks through when each
representation fits. Builds standalone and under the `cpp20` preset (29 tests,
with the `std::expected` cases compiled out).
