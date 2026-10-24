---
layout: example
order: 12
title: "Factory Method and Abstract Factory"
description: Carrier integrations built as matching product families, with the call site never naming a concrete carrier.
github_path: examples/12-factories
card_eyebrow: "Chapter 12 · Factories"
card_title: "Carrier integration: factories"
card_blurb: "An abstract factory produces a matching family of carrier products, and a factory method defers one creation step to a subclass, both removing concrete class names from the call site."
---

A carrier-integration example for two carriers (FastShip and SummitFreight):

- **Abstract Factory.** `CarrierIntegrationFactory` declares a family
  (`make_label_formatter`, `make_rate_quoter`, `make_tracking_client`); each
  concrete factory produces an internally consistent family. `ShipmentProcessor`
  builds all three from one factory and never names a concrete carrier class.
- **Factory Method.** `CarrierOnboarder::onboard()` runs a fixed sequence and
  calls a private virtual `create_test_suite()`; `SandboxOnboarder` and
  `ProductionOnboarder` override just that hook.

A C++23 `try_make_carrier_factory()` returning `std::expected` is guarded so the
C++20 build still works.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/12-factories/demo.sh cpp23
```

The demo processes a shipment through one carrier's whole product family and
onboards a carrier in sandbox and production modes. Chapter 12 distinguishes
Factory Method from Abstract Factory and when each fits. Builds standalone and
under the `cpp20` preset (12 tests).
