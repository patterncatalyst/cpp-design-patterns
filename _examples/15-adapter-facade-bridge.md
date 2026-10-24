---
layout: example
order: 15
title: "Adapter, Facade, Bridge, and PImpl"
description: One carrier integration carried through four structural techniques, from wrapping a legacy API to a compilation firewall.
github_path: examples/15-adapter-facade-bridge
card_eyebrow: "Chapter 15 · Adapter / Facade / Bridge"
card_title: "Carrier integration: structural patterns"
card_blurb: "Adapter wraps a legacy rate API, a facade fronts the rating/labeling/tracking subsystems, a bridge crosses notification kinds with transports, and PImpl hides a renderer's implementation."
---

One GlobalFreight carrier integration carried through four structural techniques:

- **Adapter.** `LegacyFreightRateAdapter` object-composes a legacy rate API,
  translating kilograms to grams and destination zones to the legacy zone codes
  so the client sees the book's `RateSource` interface.
- **Facade.** `ShippingFacade::quote_and_label()` sequences rating, labeling,
  and tracking in one call so the client does not orchestrate the subsystems.
- **Bridge.** A `Notification` abstraction crosses a `NotificationTransport`
  implementation axis: two refined kinds times two transports, four
  combinations, with no class explosion.
- **PImpl.** `LabelRenderer` hides its implementation behind a pointer; a test
  asserts `sizeof(LabelRenderer) == sizeof(void*)`.

A C++23 `try_quote_and_label()` returning `std::expected` is guarded for C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 17/17 tests pass
./examples/15-adapter-facade-bridge/demo.sh cpp23
```

The demo quotes through the adapted legacy API, produces a label through the
facade, and sends notifications across both transports. Chapter 15 distinguishes
the four techniques and when each fits. Builds standalone and under the `cpp20`
preset (15 tests).
