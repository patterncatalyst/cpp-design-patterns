---
title: "SOLID I: single responsibility, open/closed, Liskov substitution"
order: 3
part: "principles"
description: "Splitting a billing engine along the lines of what changes: one responsibility per type under the Single Responsibility Principle, a rule abstraction that is open to new tax and discount behavior without edits under the Open/Closed Principle, and a refined discount rule that honors its parent's contract under Liskov Substitution."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- State the Single Responsibility Principle as "one reason to change," not "one method," and split a class along the lines of who asks for a change rather than how many lines it has.
- Build an extension point with the Open/Closed Principle: a small interface new behavior can implement without editing the code that already calls it.
- State the Liskov Substitution Principle as a contract about behavior, not just a matching method signature, and explain what a subtype has to promise to honor it.
- Design a base class that enforces an invariant once so every subtype inherits it, rather than trusting each subtype to reimplement the same guarantee correctly.
- Read a GoogleTest suite that proves OCP and LSP directly: a test-defined type that needs no production code changes, and a function written once against a base class that behaves identically for every subtype passed to it.
- Recognize when splitting a responsibility, opening an abstraction, or refining a subtype is solving a problem nobody has yet, and say what to write instead.

## Why this matters

A small business billing system has to do three different things with the same invoice: total it, show it to the customer, and remember it in case of a dispute six weeks later. The first version often ships as one `Invoice` class, because the invoice is the object every one of those jobs is about, and that reads as the obvious place for the three jobs to live together. A sales tax rate changes once or twice a year. A receipt's layout changes whenever the business wants a different footer or a different currency format. The storage backend changes rarely, maybe once, when the business outgrows a flat file and moves to a real database. Three different reasons, three different schedules, and in the one-class version all three land in the same file, because nothing in the original design said they had to live apart.

This chapter's three principles, Single Responsibility, Open/Closed, and Liskov Substitution, respond to one force: this will change, and the code around it should not have to. Robert C. Martin names it exactly that way across all five SOLID principles, and a billing engine meets these three first, usually in that order. SRP asks which code changes for which reason, and insists one class answer to exactly one of those reasons. OCP asks what happens when a new rule shows up, a new tax jurisdiction or a new discount program, and insists the answer is a new type, not an edited one. LSP asks whether a refined version of an existing type can stand in for it everywhere that type is expected, and insists the refinement keep every promise the original made, not only its method signatures. Chapter 4 adds Interface Segregation and Dependency Inversion to this trio once a dependency crosses a boundary this chapter's three principles do not yet need to cross.

{% include excalidraw.html name="03-solid-before-after" caption="Figure 3.1: A billing engine before and after SOLID: one Invoice type doing three jobs, split by SRP into three focused types, extended by OCP through an InvoiceRule abstraction, and refined by LSP without breaking the base contract." %}

## The domain: billing an invoice

The example behind this chapter lives in `examples/03-solid-first-principles`. `LineItem` carries a description, a quantity, and a per-unit price; `Invoice` carries an id, a customer name, and a list of line items. Money is tracked as an integer `Cents`, never a `double`, so a total is exact and a test assertion can compare it without an epsilon:

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

`invoice_subtotal()` in `src/billing_domain.cpp` is the one place an invoice's line items get summed. Every responsibility introduced below calls it rather than re-deriving a subtotal of its own, the same shared-core move Chapter 8 uses for its three pricing strategies: keep the one fact every part of the system must agree on in one place, so a bug fixed there stays fixed everywhere.

## Single Responsibility: splitting by reason to change

Picture the version that has not split anything yet: one `Invoice` class with a `calculate_total()` method, a `format_receipt()` method, and a `save()` method, each reaching into the same line items and each changing for its own unrelated reason. That class compiles, runs, and passes a test the day it ships. It also means a change to the tax rate, a change to the receipt's wording, and a change to how invoices are stored all touch the same file, and a reviewer approving a tax fix has to read past formatting code and persistence code to find the three lines that matter. SRP's fix is not to make the class smaller for its own sake; it is to separate three unrelated reasons to change that were sharing one address, and give each one its own.

This example's split lands on three types, each with exactly one reason to change. `InvoiceCalculator` turns an `Invoice` and a set of rules into `Totals`:

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

`InvoiceFormatter` turns an `Invoice` and its already-computed `Totals` into receipt text, and knows nothing about how those totals were derived:

```cpp
class InvoiceFormatter {
public:
    static std::string format(const Invoice& invoice, const Totals& totals);
};
```

`InMemoryInvoiceRepository` remembers invoices by id and hands them back, and never computes or renders anything:

```cpp
class InMemoryInvoiceRepository {
public:
    void save(Invoice invoice);
    std::optional<Invoice> find_by_id(const std::string& id) const;
private:
    std::unordered_map<std::string, Invoice> invoices_by_id_;
};
```

None of these three types knows the other two exist. `InvoiceCalculator` never calls `InvoiceFormatter::format()`, and `InMemoryInvoiceRepository` never sees a `Totals` value at all; the demo in `src/demo_main.cpp` is the only place that calls all three in sequence. That separation is what makes the "one reason to change" test concrete rather than aspirational: changing the tax calculation means editing `invoice_calculator.cpp` and nothing else, changing the receipt's wording means editing `invoice_formatter.cpp` and nothing else, and a reviewer approving either change reads exactly the file the change touches, not three jobs tangled into one.

## Open/Closed: a rule abstraction instead of an edited function

A tax rate changing twice a year is a predictable kind of change, and OCP's claim is specific about how to meet it: a module should be open to having new behavior added, and closed against having its existing, already-tested code edited to make room for that behavior. The naive alternative is a function that grows a new `if` branch every time a new tax jurisdiction or discount program launches, which means every addition risks breaking a branch that was already correct and already tested. This example's answer is `InvoiceRule`, an abstraction `InvoiceCalculator` depends on and never edits:

```cpp
class InvoiceRule {
public:
    virtual ~InvoiceRule() = default;
    virtual Cents adjustment_for(Cents subtotal) const = 0;
    virtual std::string_view name() const = 0;
};
```

`PercentageTaxRule` adds a percentage of the subtotal; `PercentageDiscountRule` subtracts one, with its rate clamped to `[0, 1]`. Both existed before `FlatServiceFeeRule`, a flat addition with no relationship to the subtotal at all, joined them. Adding `FlatServiceFeeRule` touched exactly one new file. `InvoiceCalculator::totals_for()`, `PercentageTaxRule`, and `PercentageDiscountRule` are byte-for-byte what they were before it existed:

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

Every rule receives the invoice's original subtotal, not a running total updated rule by rule, so each one's adjustment is independent of the order the others ran in, and `InvoiceCalculator` never has to reason about sequencing when a new rule is added. The test suite in `tests/invoice_calculator_test.cpp` proves the "no edits" claim directly rather than describing it: it defines a brand-new `RoundUpToWholeDollarRule`, a rule type `InvoiceRule`'s own header has never seen, declares it right there in the test file, and hands an instance to `InvoiceCalculator`'s constructor. The calculator picks it up and applies it correctly with zero production code changed, because `InvoiceRule` was always the actual extension point, not a convention a developer had to remember to follow.

## Liskov Substitution: a loyalty discount that keeps its parent's promise

LSP's statement is short: a subtype must be substitutable for its base type everywhere the base type is used, without the caller noticing a difference in correctness. The part most C++ codebases miss is that "correctness" here means the base type's whole contract, not just its method signatures. A `PercentageDiscountRule` that honors `InvoiceRule`'s contract promises more than "I have an `adjustment_for()` method that returns a `Cents`"; it promises the result never discounts more than the subtotal, because every caller of `adjustment_for()` is allowed to assume that without checking.

`PercentageDiscountRule::adjustment_for()` is declared `final` specifically to protect that promise:

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

The clamp lives in exactly one place, and it runs after `discount_rate()` returns, not before. A subtype cannot override `adjustment_for()` to skip it, and every subtype's rate, however it is computed, gets clamped to `[0, 1]` before it is ever applied to a subtotal. `LoyaltyDiscountRule` refines `PercentageDiscountRule` for repeat customers, a 1%-per-year bonus capped at ten years, by overriding only the protected `discount_rate()` hook:

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

A loyalty customer with a 95% negotiated base rate and fifty years on the account would, unclamped, produce a combined rate past 100%, discounting more than the invoice is worth. `LoyaltyDiscountRule` never has to defend against that case itself, because `adjustment_for()`'s clamp runs on whatever `discount_rate()` returns, including a value over 1.0. `tests/invoice_lsp_test.cpp` makes the substitutability claim concrete with one function, written once against the base class:

```cpp
Cents adjustment_through_base_class(const InvoiceRule& rule, Cents subtotal) {
    return rule.adjustment_for(subtotal);
}
```

That function is called with a `PercentageDiscountRule` and with a `LoyaltyDiscountRule`, and both calls behave correctly, because neither subtype had the chance to violate the invariant the base class owns. This is also why the test that stores several discount rules together for comparison holds them by `std::reference_wrapper<const PercentageDiscountRule>`, not by value in an array: an array of `PercentageDiscountRule` values would slice a `LoyaltyDiscountRule` down to its parent on the way in, silently discarding the override under test and defeating the substitution the test exists to check. Holding subtypes by reference or by pointer, never by value, is what lets a base-typed container hold more than one concrete type.

## Typed validation: C++20 versus C++23

`InvoiceCalculator::totals_for()` has no way to reject an invoice with no line items; it returns a zero total for one as readily as it returns a real total for a stocked one, and a caller has no signal that anything was unusual. C++23's `std::expected` gives `InvoiceCalculator` a second entry point that can say why a request was rejected, without the caller wrapping every call in a `try` block the way an exception-based contract would require.

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

Both columns reject the same condition on the same subtotal path; only the shape of the rejection changes between standards, the same pattern Chapter 8 used for a malformed shipment. Neither column touches SRP, OCP, or LSP: `InvoiceCalculator` still has exactly one reason to change, `InvoiceRule` is still the only extension point, and every `InvoiceRule` subtype is still substitutable through the same base-class reference. The standard a build targets is a decision orthogonal to the three principles this chapter trains: a newer standard changes how a failure is reported, not whether a class has one reason to change, whether a rule can be added without edits, or whether a subtype keeps its parent's promise.

> **AI sidebar.** Ask an LLM pairing partner to "apply SOLID" to a working `Invoice` class and a common result is a `IInvoiceCalculator` interface with exactly one implementation, a `IInvoiceFormatter` interface with exactly one implementation, and a factory to construct both, because that shape appears constantly in tutorials that treat "SOLID" as a checklist to satisfy rather than a response to a specific force. Check the result the way this chapter trained: SRP asks whether the split follows an actual difference in who changes the code and why, not just whether the result has more files than before. OCP asks whether a second rule, implementation, or variant is in hand or clearly coming; an interface with one implementation and nothing else on the roadmap is OCP's cost with none of its benefit. LSP asks whether a generated subtype honors its parent's invariants, not merely its method signatures; a model asked to "add a specialized version" of a class will cheerfully override a method in a way that breaks an invariant the base class never wrote down anywhere a generator could see it, which is exactly why this chapter's `PercentageDiscountRule` enforces its clamp in a `final` method instead of trusting every future subtype to remember it.

## When SOLID works against you

Every principle in this chapter earns its cost only when the force behind it is present, and all three share the same failure mode in the other direction: applying them because the name is familiar, not because the forces that justify the name are in the code.

A billing engine with one tax rule, one discount program, and no second one on any roadmap does not need an `InvoiceRule` interface; it needs a function. Wrapping `tiered_zone_cost()`-style math in a virtual interface before a second implementation exists adds a vtable, an extra header, and a reader's detour through an abstraction to find the one case that will ever run through it, with nothing on the other side of that cost to justify it yet. The same caution applies to splitting responsibilities: an `Invoice` class with one `calculate_total()` method and no formatting or persistence code anywhere near it does not need `InvoiceCalculator` carved out as a separate type in advance of a second responsibility arriving; premature separation here reads as more files to open to understand one calculation, not less.

LSP's version of this failure is subtler and more common: building a base class with a protected extension hook, the way `PercentageDiscountRule::discount_rate()` exists here, when only one subtype will ever override it. That hook is worth its design cost in this chapter's example because a second, materially different discount rule, the loyalty variant, was already planned; absent that, `PercentageDiscountRule` could apply its own rate directly with no hook at all, and a hook with one caller is a promise of flexibility nobody is going to collect on. The test for all three principles in this chapter is the same test Chapter 1 trained for every pattern: name the abstraction and the plain alternative side by side, and let whether a second case is in hand or clearly coming decide between them, not the principle's reputation for being good practice in general.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/03-solid-first-principles` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 17 cases under the `cpp23` preset, 15 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 17   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

To see all three principles play out on one invoice rather than just reading an assertion that says they hold, run the example's own demo script:

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

Running `examples/03-solid-first-principles/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the Single Responsibility, Open/Closed, and Liskov Substitution principles this chapter builds on, and the source of the "reason to change" framing this chapter and Chapter 4 both use.
- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Gang of Four's own advice to favor composition and small, focused abstractions, the same instinct OCP's `InvoiceRule` and LSP's clamped base class both express in idiomatic C++.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
