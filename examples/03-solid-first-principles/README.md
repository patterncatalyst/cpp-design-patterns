# 03 — SOLID I: billing an invoice

A small business billing system needs to total an invoice, show it to a
customer, and remember it between runs. This example builds that system
three SOLID principles deep: SRP keeps calculation, formatting, and
persistence in three separate types; OCP lets a new kind of pricing rule
join the total without editing the calculator; LSP keeps a refined discount
rule substitutable everywhere its parent is expected.

## The domain

- `LineItem` — a description, a quantity, and a per-unit price.
- `Invoice` — an id, a customer name, and a list of `LineItem`s.
- Money is tracked as integer `Cents`, never floating point, so a total is
  exact and reproducible in a test assertion.
- `invoice_subtotal()` in `src/billing_domain.cpp` is the one place line
  items are summed; every responsibility below calls it rather than
  re-deriving a subtotal of its own.

## Single Responsibility: three types, three jobs

- **`InvoiceCalculator`** (`src/invoice_calculator.hpp`) — turns an
  `Invoice` plus a set of rules into `Totals` (a subtotal and a total). It
  has no idea how a total is displayed or whether it is ever saved.
- **`InvoiceFormatter`** (`src/invoice_formatter.hpp`) — turns an `Invoice`
  and its already-computed `Totals` into receipt text. It never computes a
  total itself.
- **`InMemoryInvoiceRepository`** (`src/invoice_repository.hpp`) —
  remembers invoices by id and hands them back. It never computes or
  renders anything.

Each type has exactly one reason to change: a new tax rate changes
`InvoiceCalculator`, a new receipt layout changes `InvoiceFormatter`, a new
storage backend changes `InMemoryInvoiceRepository` — never all three for
one requirement.

## Open/Closed: `InvoiceRule` as the extension point

`InvoiceRule` (`src/invoice_rules.hpp`) is the abstraction OCP extends
instead of editing: anything that turns a subtotal into an adjustment is a
rule, and `InvoiceCalculator` never changes to pick up a new one.

- `PercentageTaxRule` — adds a percentage of the subtotal.
- `PercentageDiscountRule` — subtracts a percentage, with its rate clamped
  to `[0, 1]` so no caller can push it into discounting more than the
  subtotal.
- `FlatServiceFeeRule` — a fixed addition unrelated to the subtotal at all,
  added after the two rules above already existed, with zero changes to
  `InvoiceCalculator` or to either of them.
- `tests/invoice_calculator_test.cpp` goes one step further: it defines a
  brand-new `RoundUpToWholeDollarRule` **inside the test file itself** and
  proves `InvoiceCalculator` picks it up with no production code touched at
  all.

## Liskov Substitution: a loyalty discount that keeps its parent's promise

`PercentageDiscountRule::adjustment_for()` is declared `final`: it always
clamps its rate to `[0, 1]` and applies it in exactly one place, so every
subtype inherits the invariant "never discount more than the clamped rate
times the subtotal" instead of reimplementing it.

`LoyaltyDiscountRule` refines `PercentageDiscountRule` for repeat customers
(a 1%-per-year bonus, capped at 10 years) by overriding only the protected
`discount_rate()` hook. It is substitutable anywhere a
`PercentageDiscountRule` or an `InvoiceRule` is expected —
`tests/invoice_lsp_test.cpp` calls both through a single function written
against the `InvoiceRule` base class and shows the invariant holds for both,
even when a loyalty bonus would otherwise push the combined rate above 100%.

### C++23 refinement (guarded)

`InvoiceCalculator::try_totals_for()` returns
`std::expected<Totals, std::string>`, rejecting an invoice with no line
items with a typed reason instead of silently returning a zero total. It is
compiled only under C++23 (`#if __cplusplus >= 202302L` in
`src/invoice_calculator.hpp` and `.cpp`), so the mandatory C++20 build path
never needs `std::expected`. The matching gtest cases in
`tests/invoice_calculator_test.cpp` are guarded the same way.

## Build, run, observe

No containers — local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/03-solid-first-principles
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild — from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 17 gtest cases under `cpp23` (15 under
`cpp20` — the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 17   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

To see all three principles play out on one invoice rather than just
reading an assertion that says they hold, run the example's own demo
script:

```sh
examples/03-solid-first-principles/demo.sh cpp23
```

It prices a two-line invoice for Riverside Hardware Co. (a $2,500.00
website design plus three months of $15.00 hosting), persists and reloads
it, breaks down each rule's adjustment, and then swaps the standard
discount for a loyalty discount on the same calculator:

```
== Single Responsibility: calculate, format, persist ==
Invoice INV-1042 - Riverside Hardware Co.
  1 x Website design @ $2500.00 = $2500.00
  3 x Hosting (monthly) @ $15.00 = $45.00
  Subtotal: $2545.00
  Total:    $2525.46
Persisted and reloaded by id: yes

== Open/Closed: three rule kinds, one calculator never edited ==
sales tax: $209.96
percentage discount: -$254.50
service fee: $25.00
Invoice total: $2525.46

== Liskov Substitution: a loyalty discount stands in for the base rule ==
percentage discount: -$254.50
loyalty discount: -$178.15
Invoice total for a 4-year loyalty customer: $2601.81

== C++23 refinement: typed validation via std::expected ==
Rejected empty invoice: invoice has no line items to total
```

Running `examples/03-solid-first-principles/demo.sh cpp20` prints the same
output minus the final `std::expected` block, which only compiles under the
`cpp23` preset.

## Files

```
examples/03-solid-first-principles/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    billing_domain.hpp/.cpp      # LineItem, Invoice, Cents, shared subtotal
    invoice_rules.hpp/.cpp       # OCP + LSP: InvoiceRule and its subtypes
    invoice_calculator.hpp/.cpp  # SRP: calculation
    invoice_formatter.hpp/.cpp   # SRP: formatting
    invoice_repository.hpp/.cpp  # SRP: persistence
    demo_main.cpp                 # exercises all three principles
  tests/
    invoice_calculator_test.cpp  # SRP + OCP (test-defined rule type)
    invoice_formatter_test.cpp   # SRP
    invoice_repository_test.cpp  # SRP
    invoice_lsp_test.cpp         # LSP
```
