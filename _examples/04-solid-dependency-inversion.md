---
layout: example
order: 4
title: "SOLID II: dependency inversion"
description: An order-notification service built on segregated role interfaces and inverted dependencies, with hand-written test doubles.
github_path: examples/04-solid-dependency-inversion
card_eyebrow: "Chapter 4 · SOLID II"
card_title: "Order notifications: ISP & DIP"
card_blurb: "A high-level policy depends on an abstraction, not a concrete gateway, and a fat interface is split into role interfaces each client depends on only where it needs them."
---

An order-notification service that shows interface segregation, dependency
inversion, and dependency injection without a framework:

- **Interface segregation.** A fat gateway is split into role interfaces
  (`MessageSender`, `TemplateRenderer`, `DeliveryStatusProvider`). `EmailGateway`
  implements all three; `SmsGateway` implements only the one it can.
- **Dependency inversion.** `OrderNotificationPolicy` depends on the abstract
  roles, not on any concrete gateway. The only file that names a concrete type
  is the composition root in `demo_main.cpp`.
- **DI without a framework.** Dependencies are injected by constructor and the
  object graph is assembled in one place. The tests use hand-written fakes (no
  mock framework) to prove the policy drives the abstraction correctly.

A C++23 `try_notify_customer()` returning `std::expected` falls back to
`std::optional` under C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/04-solid-dependency-inversion/demo.sh cpp23
```

The demo dispatches an order notification through the injected gateway and
audits delivery status through a segregated role interface. Chapter 4 walks
through why the dependency arrow is inverted and when a container helps versus
hurts. Builds standalone and under the `cpp20` preset (14 tests).
