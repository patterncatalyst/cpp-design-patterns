---
layout: example
order: 18
title: "Observer and Mediator"
description: Shipment-status notifications fanned out to observers, and a dispatch coordinator that keeps collaborators from referencing each other.
github_path: examples/18-observer-mediator
card_eyebrow: "Chapter 18 · Observer / Mediator"
card_title: "Shipment tracking: observe and mediate"
card_blurb: "A tracker fans status changes out to interface and std::function observers with RAII subscriptions, and a mediator routes between warehouse, carrier, and customer stations that never know each other."
---

Two coordination patterns:

- **Observer.** A `ShipmentTracker` fans a status change out to interface-based
  observers (`DashboardView`, `AlertingService`) and a plain `std::function`
  audit callback. `subscribe()` returns a move-only RAII `ScopedSubscription`
  that unsubscribes in its destructor; notification runs over a snapshot of the
  subscriber list so a slot can unsubscribe mid-callback safely.
- **Mediator.** A `DispatchCoordinator` routes between `WarehouseStation`,
  `CarrierStation`, and `CustomerNotificationStation`, none of which reference
  each other. A cascade (order ready → pickup confirmed → customer notified)
  shows many-to-many routing through one hub.

A C++23 `try_subscribe()` returning `std::expected` (rejecting a subscribe to a
terminal shipment) is guarded, with a throwing C++20 fallback.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/18-observer-mediator/demo.sh cpp23
```

The demo advances a shipment and shows each observer react, drops a subscription
and shows the fan-out shrink, then runs a mediated dispatch cascade. Chapter 18
distinguishes Observer from Mediator and covers observer lifetime. Builds
standalone and under the `cpp20` preset (12 tests).
