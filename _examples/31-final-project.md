---
layout: example
order: 31
title: "Final project: composing patterns into a system"
description: A shipment dispatch console composing Strategy, Factory, State, Command with undo, and Observer into one coherent system, proven by an end-to-end integration test.
github_path: examples/31-final-project
card_eyebrow: "Chapter 31 · Final project"
card_title: "Five patterns, one system"
card_blurb: "A shipment dispatcher that composes Factory, Strategy, State, Command with undo, and Observer. The integration test proves the composition: a lifecycle run fires observer events in order, a command re-prices through the factory-built strategy, and undo restores the prior state."
---

The book's final project: one shipment dispatch console composing five patterns
the book already built, taught through the seams where they meet:

- **Factory → Strategy.** `CarrierFactory` builds a carrier profile with the
  matching `RateStrategy` from a request.
- **Command → State.** `AdvanceLifecycleCommand` drives the shipment through
  Created → PickedUp → InTransit → Delivered, with undo restoring a snapshot
  rather than firing a reverse event.
- **Command → Strategy.** `AdjustWeightCommand` re-prices through the carrier's
  strategy and restores the exact prior quote on undo.
- **State → Observer.** Every legal transition and every undo-driven correction
  fans out to `LifecycleObserver` subscribers.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 34/34 tests pass
./examples/31-final-project/demo.sh cpp23
```

The integration test (`shipment_dispatcher_integration_test.cpp`) is the
composition proof: a factory-built strategy prices the opening quote, two
lifecycle commands fire GoogleMock-verified observer notifications in sequence, a
weight correction re-prices, and `undo_last()` restores weight, quote, and status
across the whole chain. The factory's rejection path returns `std::expected` on
C++23 and throws on C++20. Builds standalone and under the `cpp20` preset
(32 tests).
