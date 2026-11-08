---
title: "SOLID I: single responsibility, open/closed, Liskov substitution"
order: 3
part: "principles"
description: "Splitting a billing engine along the lines of what changes: one responsibility per type under the Single Responsibility Principle, a rule abstraction that is open to new tax and discount behavior without edits under the Open/Closed Principle, and a refined discount rule that honors its parent's contract under Liskov Substitution."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- State the Single Responsibility Principle as "one reason to change," and split a class by who asks for the change.
- Build an Open/Closed extension point: a small interface new behavior implements without editing its caller.
- State the Liskov Substitution Principle as a contract about behavior, not a matching signature.
- Enforce an invariant once in a base class so no subtype can skip it.
- Say when a split, an abstraction or a subtype hook solves a problem nobody has yet.

## Why this matters

A small business bills with one `Invoice` class that totals the invoice, formats the receipt and saves it. The tax rate changes once or twice a year. The receipt layout changes whenever the business wants a new footer or currency format. Storage changes once, when the business outgrows a flat file. Three reasons to change, on three schedules, all landing in one file.

Robert C. Martin's three principles here answer "this will change, and the code around it shouldn't have to":

| Principle | Asks | Answer |
| --- | --- | --- |
| Single Responsibility (SRP) | which code changes for which reason? | one type per reason |
| Open/Closed (OCP) | what happens when a new tax or discount rule arrives? | a new type, not an edited one |
| Liskov Substitution (LSP) | can a refined type stand in for its parent everywhere? | only if it keeps every promise the parent made |

Chapter 4 adds Interface Segregation and Dependency Inversion once a dependency crosses a boundary.

{% include excalidraw.html name="03-solid-before-after" caption="Figure 3.1: A billing engine before and after SOLID: one Invoice type doing three jobs, split by SRP into three focused types, extended by OCP through an InvoiceRule abstraction, and refined by LSP without breaking the base contract." %}

## The domain: billing an invoice

The code is in `examples/03-solid-first-principles`. Money is an integer `Cents`, never a `double`, so totals are exact:

```cpp
using Cents = std::int64_t;

struct LineItem {
    std::string description;
    int quantity;
    Cents unit_price_cents;
};

struct Invoice {
    std::string id;
    std::string customer_name;
    std::vector<LineItem> line_items;
};
```

`invoice_subtotal()` in `src/billing_domain.cpp` is the one place line items are summed. Everything below calls it, so a fix there applies everywhere.

## Single Responsibility: split by reason to change

The unsplit version is one `Invoice` class with `calculate_total()`, `format_receipt()` and `save()`. A tax fix, a wording change and a storage change all touch the same file, and the reviewer of a tax fix reads past formatting and persistence code to find three relevant lines. SRP separates the three reasons; it doesn't make classes small for their own sake.

`InvoiceCalculator` turns an `Invoice` and a set of rules into `Totals`:

```cpp
struct Totals {
    Cents subtotal;
    Cents total;
};

// A non-owning reference to a rule the calculator applies but does not
// own; the caller keeps each rule alive.
using RuleRef = std::reference_wrapper<const InvoiceRule>;

class InvoiceCalculator {
public:
    explicit InvoiceCalculator(std::vector<RuleRef> rules);
    Totals totals_for(const Invoice& invoice) const;
    // ...
private:
    std::vector<RuleRef> rules_;
};
```

`InvoiceFormatter` turns an invoice and its computed totals into receipt text:

```cpp
class InvoiceFormatter {
public:
    static std::string format(const Invoice& invoice, const Totals& totals);
};
```

`InMemoryInvoiceRepository` stores and finds invoices by id:

```cpp
class InMemoryInvoiceRepository {
public:
    void save(Invoice invoice);
    std::optional<Invoice> find_by_id(const std::string& id) const;
private:
    std::unordered_map<std::string, Invoice> invoices_by_id_;
};
```

None of the three knows the others exist; only `src/demo_main.cpp` calls all three. A tax change edits `invoice_calculator.cpp`, a wording change edits `invoice_formatter.cpp`, and each review reads one file.

## Open/Closed: a rule abstraction, not an edited function

OCP says a module should be open to new behavior and closed to edits of its tested code. The naive version is a function that grows an `if` for every new jurisdiction or discount program, so every addition risks a branch that already worked. Here `InvoiceCalculator` depends on `InvoiceRule` and never changes:

```cpp
class InvoiceRule {
public:
    virtual ~InvoiceRule() = default;
    virtual Cents adjustment_for(Cents subtotal) const = 0;
    virtual std::string_view name() const = 0;
};
```

| Rule | Adjustment |
| --- | --- |
| `PercentageTaxRule` | adds a percentage of the subtotal |
| `PercentageDiscountRule` | subtracts a percentage, rate clamped to `[0, 1]` |
| `FlatServiceFeeRule` | adds a flat amount, unrelated to the subtotal |

`FlatServiceFeeRule` arrived last. Adding it added a class next to the other rules; `InvoiceCalculator::totals_for()` and the two percentage rules didn't change:

```cpp
Totals InvoiceCalculator::totals_for(const Invoice& invoice) const {
    const Cents subtotal = invoice_subtotal(invoice);
    Cents total = subtotal;
    for (const InvoiceRule& rule : rules_) {
        total += rule.adjustment_for(subtotal);
    }
    return Totals{subtotal, total};
}
```

Every rule receives the original subtotal, not a running total, so the order of rules never changes the result. A rule type defined entirely outside production code, such as one that rounds up to the whole dollar, plugs into the calculator with no production change.

## Liskov Substitution: a loyalty discount that keeps its parent's promise

A subtype must be usable wherever its base is, with no difference in correctness. Correctness means the whole contract, not the signatures. `PercentageDiscountRule` promises more than "returns a `Cents`": it promises never to discount more than the subtotal, and every caller may rely on that without checking.

`PercentageDiscountRule::adjustment_for()` is `final` to protect that promise:

{% include excalidraw.html name="03-lsp-clamp" caption="Figure 3.2: subtotal flows through the overridable discount_rate() hook into a clamp that adjustment_for() owns and never exposes for override, so every subtype's rate is bounded before it reaches the subtotal." %}

```cpp
class PercentageDiscountRule : public InvoiceRule {
public:
    explicit PercentageDiscountRule(double rate);
    Cents adjustment_for(Cents subtotal) const final;
    std::string_view name() const override;

protected:
    virtual double discount_rate(Cents subtotal) const;

private:
    double rate_;
};
```

```cpp
Cents PercentageDiscountRule::adjustment_for(Cents subtotal) const {
    const double clamped_rate = clamp_rate(discount_rate(subtotal));
    return -static_cast<Cents>(std::llround(static_cast<double>(subtotal) * clamped_rate));
}
```

The clamp lives in one place and runs on whatever `discount_rate()` returns. A subtype can't override `adjustment_for()` to skip it. `LoyaltyDiscountRule` adds 1% per year, capped at ten years, by overriding only the protected hook:

```cpp
class LoyaltyDiscountRule : public PercentageDiscountRule {
public:
    LoyaltyDiscountRule(double base_rate, int loyalty_years);
    std::string_view name() const override;

protected:
    double discount_rate(Cents subtotal) const override;

private:
    double bonus_rate_;
};

double LoyaltyDiscountRule::discount_rate(Cents subtotal) const {
    return PercentageDiscountRule::discount_rate(subtotal) + bonus_rate_;
}
```

A 95% negotiated base rate plus ten loyalty years gives 105%, more than the invoice is worth. `LoyaltyDiscountRule` never defends against that itself; the base class's clamp does. A function written once against `const InvoiceRule&` gets a correct adjustment from either rule.

Hold subtypes by reference or pointer when they share a container. An array of `PercentageDiscountRule` values slices a `LoyaltyDiscountRule` down to its parent on the way in and silently drops the override; a `std::reference_wrapper<const PercentageDiscountRule>` keeps it.

## Typed validation: C++20 versus C++23

`totals_for()` returns a zero total for an invoice with no line items, with no signal that anything was wrong. `std::expected`, new in C++23, gives `InvoiceCalculator` a second entry point that says why it rejected a request, without exceptions.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: no equivalent entry point exists in the mandatory build path.
// A caller that needs to reject an invalid invoice falls back to
// std::optional, which loses the reason for the rejection: the caller
// learns *that* the invoice was rejected, not *why*.
std::optional<Totals> try_totals_for(const Invoice& invoice) const {
    if (invoice.line_items.empty()) {
        return std::nullopt;
    }
    return totals_for(invoice);
}
```
```cpp
// C++23: std::expected carries a typed reason alongside the success
// value, checkable with an if and no exception machinery. This is the
// version InvoiceCalculator actually ships, guarded behind
// #if __cplusplus >= 202302L so the mandatory C++20 build path never
// needs std::expected.
std::expected<Totals, std::string> try_totals_for(const Invoice& invoice) const {
    if (invoice.line_items.empty()) {
        return std::unexpected<std::string>("invoice has no line items to total");
    }
    return totals_for(invoice);
}
```

Both reject the same condition; only the shape of the rejection changes. The standard changes how a failure is reported, not how the responsibilities split, where the extension point is, or whether a subtype keeps its parent's promise.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a SOLID refactor.** "Apply SOLID to `Invoice`" doesn't say which reasons to change justify a split, whether a second rule exists, or which invariant a subtype must keep. State those:

> In `examples/03-solid-first-principles`, keep `InvoiceCalculator`, `InvoiceFormatter` and `InMemoryInvoiceRepository` as three concrete types with no `I`-prefixed interfaces. Add a new rule as a class deriving from `InvoiceRule` in `src/invoice_rules.hpp` without editing `InvoiceCalculator::totals_for()`. Any new discount subtype derives from `PercentageDiscountRule` and overrides only `discount_rate()`; `adjustment_for()` stays `final` and keeps the clamp to `[0, 1]`.

Check the reply with `ctest --preset cpp23 -R 'InvoiceCalculator|LiskovSubstitution'`, and reject any diff that touches `totals_for()` or removes `final`.
</blockquote>

## When SOLID works against you

Each principle pays only when its force is present:

| Principle | Skip it when | Write instead |
| --- | --- | --- |
| SRP | an `Invoice` has `calculate_total()` and no formatting or persistence code nearby | the one class; split when a second responsibility arrives |
| OCP | there's one tax rule, one discount and no second one on any roadmap | a function, not an `InvoiceRule` interface with a vtable and an extra header |
| LSP hook | only one subtype will ever override `discount_rate()` | `PercentageDiscountRule` applying its own rate directly, with no protected hook |

The hook is worth it here because the loyalty variant was already planned. As Chapter 1 puts it: name the abstraction and the plain alternative, and let whether a second case is in hand decide.

## Build, run, observe

From the repository root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 17 cases pass under `cpp23` and 15 under `cpp20`; the two `std::expected` cases compile only under C++23. Then run the demo:

```sh
examples/03-solid-first-principles/demo.sh cpp23
```

It prices a two-line invoice for Riverside Hardware Co. (a $2,500.00 website design plus three months of $15.00 hosting), persists and reloads it by id, breaks down each rule's adjustment, and then swaps the standard discount for a loyalty discount on the same calculator:

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

Under `cpp20` the demo prints the same output without the final `std::expected` block.

One experiment shows what the `final` clamp buys:

- **Drop the clamp.** In a scratch copy, change `adjustment_for()` to use `discount_rate(subtotal)` without `clamp_rate`. One case fails: a loyalty discount at the ten-year cap now exceeds the subtotal.

## For deeper coverage

- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the Single Responsibility, Open/Closed, and Liskov Substitution principles this chapter builds on, and the source of the "reason to change" framing this chapter and Chapter 4 both use.
- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Gang of Four's own advice to favor composition and small, focused abstractions, the same instinct OCP's `InvoiceRule` and LSP's clamped base class both express in idiomatic C++.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
