---
layout: example
order: 14
title: "Singleton and the DI alternative"
description: A shipment audit log built first as a Meyers singleton, then as an injected abstraction, with the testability contrast made concrete.
github_path: examples/14-singleton-and-di
card_eyebrow: "Chapter 14 · Singleton"
card_title: "Audit log: singleton vs DI"
card_blurb: "A thread-safe Meyers singleton shown honestly alongside its costs, then the same capability as a constructor-injected abstraction that a test can swap for a fake."
---

A shipment-fulfillment audit log built two ways so the trade-off is concrete:

- **Singleton.** `ShipmentAuditLog::instance()` is a thread-safe Meyers
  singleton (function-local static). Its `reset_for_testing()` escape hatch is
  itself evidence of the global-state cost: tests cannot otherwise isolate it.
- **Dependency injection.** An `AuditSink` interface with an
  `InMemoryAuditSink` implementation, injected by constructor into
  `OrderFulfillmentService` and wired by the composition root in `demo_main.cpp`
  (reusing Chapter 4's manual-DI shape).

The DI tests inject a fake sink and verify behavior the singleton version cannot
easily reach. A C++23 `try_fulfill_order()` returning `std::expected` falls back
to `std::optional` under C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 9/9 tests pass
./examples/14-singleton-and-di/demo.sh cpp23
```

The demo logs through the singleton, then fulfills an order through the injected
sink and inspects what the sink recorded. Chapter 14 walks through when a
singleton is defensible and when injection is the better default. Builds
standalone and under the `cpp20` preset (9 tests).
