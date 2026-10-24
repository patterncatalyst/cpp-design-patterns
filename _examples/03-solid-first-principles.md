---
layout: example
order: 3
title: "SOLID I: an invoicing engine"
description: A billing component that improves as SRP, OCP, and LSP are applied, proven by GoogleTest.
github_path: examples/03-solid-first-principles
card_eyebrow: "Chapter 3 · SOLID I"
card_title: "Invoicing: SRP, OCP, LSP"
card_blurb: "One Invoice type doing three jobs, split into focused responsibilities, extended through a rule abstraction without edits, and refined by a subtype that honors the base contract."
---

A small invoicing/billing component that shows the first three SOLID
principles as a design that visibly improves:

- **Single responsibility.** `InvoiceCalculator`, `InvoiceFormatter`, and an
  `InMemoryInvoiceRepository` each do one job and none knows the others exist.
- **Open/closed.** `InvoiceRule` is the extension point. New rules
  (`PercentageTaxRule`, `PercentageDiscountRule`, `FlatServiceFeeRule`) are
  added without editing the calculator. A test even defines its own
  `RoundUpToWholeDollarRule` and the calculator picks it up with no production
  change.
- **Liskov substitution.** `LoyaltyDiscountRule` refines
  `PercentageDiscountRule` by overriding only a protected hook, so a combined
  rate still clamps correctly and the subtype is substitutable through the base
  interface.

Money is integer `Cents`, never floating point. A C++23 `try_totals_for()`
returning `std::expected` is guarded so the C++20 build still works.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 17/17 tests pass
./examples/03-solid-first-principles/demo.sh cpp23
```

The demo prints a worked invoice (subtotal `$2545.00`, a standard total of
`$2525.46`, and a loyalty total of `$2601.81`). Chapter 3 walks through how each
principle reshapes the design. Builds standalone and under the `cpp20` preset
(15 tests, with the `std::expected` case compiled out).
