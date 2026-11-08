---
title: "Refactor or rewrite"
order: 30
part: "changing-inherited-code"
description: "Choosing between incremental refactoring and a bounded rewrite, taught against a legacy freight-pricing engine with an undocumented zone-rate matrix, two undocumented customer overrides and an unexplained rounding rule. A strangler fig router replaces it behind a stable PricingEngine interface, customer by customer, after a shared characterization table shows the new engine agrees with the old."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the factors that decide refactor versus rewrite: risk, cost, undocumented knowledge in the code, how well it's understood, whether a safety net exists, and the cost of being wrong.
- Explain why a rewrite from scratch usually loses.
- Recognize when a bounded rewrite wins, and state the conditions that make the call defensible.
- Describe the strangler fig: a stable interface, a runtime routing seam, and traffic shifted as confidence grows.
- Build a characterization table both old and new implementations must pass, and use a compare mode to catch a disagreement before a customer sees it.
- Judge when a direct change or a full swap beats the seam and gradual cutover.

{% include excalidraw.html name="30-refactor-or-rewrite" caption="Figure 30.1: callers depend on PricingEngine, never on a concrete engine; StranglerPricingRouter sits behind that stable interface and shifts traffic from LegacyPricingEngine toward ModernPricingEngine release by release, with a compare mode running during the verification window." %}

## Why this matters

Every inherited codebase eventually has a component nobody wants to touch: a pricing engine, a tax calculator, a fraud scorer. It has worked for years, nobody knows why every branch exists, and the person who might have known left two reorganizations ago. The team has to choose: keep refactoring it in place one safe step at a time, or build a replacement from a cleaner start?

Both answers are right some of the time. A team that always refactors will sometimes polish a design that should have been replaced. A team that always rewrites will sometimes throw away years of edge-case handling and spend a year reimplementing it from memory. The choice needs a repeatable set of factors, and when the answer is a bounded rewrite of a system that can't stop serving traffic, it needs the strangler fig.

## The decision factors

Age and ugliness aren't on the list. Old, ugly code that is well tested and understood is a good refactoring candidate. Code written last quarter that nobody can explain and nothing tests is a rewrite candidate.

| Factor | What to ask | Which way it pushes |
| --- | --- | --- |
| **Risk** | What breaks if it misbehaves, and who notices first? An engine that silently undercharges for six months is riskier than one that fails loudly. | High risk favors small, reversible steps: refactoring, or an incremental strangler cutover. |
| **Cost** | What shape is the spending? Refactoring spreads time out while the system ships. A rewrite concentrates it, and the business keeps changing the old system, so changes land twice. | Concentrated spending with a long window favors refactoring. |
| **Undocumented knowledge** | Does the code hold rules written down nowhere else, such as a zone-rate matrix built from years of lane negotiations? | Teams underweight this one most. The more there is, the more a rewrite must preserve it as requirements, not clean it up as noise. |
| **Understanding** | Can people on the team explain it line by line, or is "removing that branch broke something once" the documentation? | Less understanding makes a rewrite a bet on re-deriving knowledge already paid for once. Characterization tests turn tribal knowledge into an executable fact. |
| **Safety net** | Do tests cover it? | Without them, refactoring isn't conservative: it changes the one thing you've seen working with no way to detect when it stops. Building the net comes first on either path. |
| **Cost of being wrong** | A wrong refactor wastes time on a slightly better component. A wrong rewrite leaves a half-built replacement and a legacy system untouched for a year and less understood. | The asymmetry favors refactoring unless a specific force argues otherwise. |

Feathers' definition of legacy code as code without tests matters for the knowledge factor: the missing tests usually come with missing documentation, so the code's behavior is the only surviving record of what the business agreed with a customer. A rewrite that ignores that record ships something that compiles and reviews well and stops honoring a contract term nobody remembered outside the code.

## Why rewrite from scratch usually loses

"Let's just rewrite it" loses for three repeatable reasons:

- **Lost embedded knowledge.** A legacy engine's quirks are rarely bugs; they're fossilized decisions. A discount as a flat dollar amount, a surcharge above a declared value, a rounding rule from a retired system: each was put there because a contract, a downstream system or a regulation needed it. A rewrite built from a clean model of "how pricing should work" replaces that with the author's unvalidated idea of it.
- **Dual maintenance.** The business doesn't stop changing the old system. Every fix, contract and regulatory change lands there, then lands again in the rewrite or gets forgotten, so the rewrite is behind before it ships. This cost is invisible during planning and surfaces months in as "did the new system get the Q3 pricing change?"
- **A long window with no delivery.** Nothing pays off until the whole replacement is done and cut over, and nobody else waits for that date. Incremental refactoring ships value with every step.

A bounded rewrite earns its cost when the problems are structural: the language or platform is the limit, the architecture blocks a change the business needs, or the code is so poorly understood and tested that incremental change is the riskier path. Even then it needs a way to ship without one all-or-nothing cutover day.

## The strangler fig approach

Martin Fowler named the technique after the strangler fig, a tree that germinates in a host tree's canopy, sends roots to the ground and envelops the host until it can stand alone. In software, a new implementation replaces a legacy component behind a stable interface, taking more traffic as confidence grows. The legacy component is removed only once the new one meets the same bar.

Three pieces make it work:

- **A stable interface.** Callers depend on an abstraction, `quote()`, never on a concrete engine, so the implementation behind it can change without touching a call site.
- **A routing seam.** Something decides, per request, which implementation answers. It runs in three modes: legacy-only, the start and the rollback target; compare, which runs both and logs disagreements without ever returning the new answer to a customer; and routed, which sends specific traffic, such as a customer, a tenant or a percentage, to the new implementation.
- **A shared characterization table.** The new implementation earns traffic by passing every case that pins the old one's behavior, including undocumented rules. A newly written suite shows the code does what its author meant; the old cases show it does what the business needs, which is the stronger claim.

The gradual shift is what makes it a strangler fig rather than a facade. The legacy share shrinks one customer, region or percentage point at a time, each step observable and reversible. When the last traffic moves, the legacy component can be deleted and nobody in production notices, because the absence of a difference was already shown.

## The migration, worked in code

`PricingEngine` is the stable interface: one method, `quote()`, that every engine implements. `LegacyPricingEngine` prices a lane between five regional hubs from a zone-rate matrix grown by hand over years of negotiations, with no formula behind it:

{% raw %}
```cpp
const std::map<std::pair<std::string, std::string>, int>& zone_matrix() {
    static const std::map<std::pair<std::string, std::string>, int> table{
        {{"NE", "NE"}, 400},  {{"MW", "MW"}, 350},  {{"SE", "SE"}, 300},
        // ... twelve more lanes, none of them a formula
    };
    return table;
}
```
{% endraw %}

Two customer overrides sit beside it as one-off branches, not a shared discount mechanism:

```cpp
if (customer_id == "ACME-2019" && subtotal_cents > 2000) {
    return subtotal_cents - 500;
}
if (customer_id == "GLOBEX") {
    return static_cast<int>(subtotal_cents * 0.90);
}
```

Nothing marks either branch as a negotiated contract term. You have to know ACME-2019's volume rebate exists to recognize the code that enforces it. A final step rounds every total up to the nearest quarter dollar, inherited from a decommissioned mainframe invoicing system. Nobody can confirm whether the consumer of quarter-rounded totals is still in service, so the rewrite keeps the rule.

`tests/pricing_characterization_cases.hpp` pins all of this: seven shipment profiles and the exact totals `LegacyPricingEngine` returns today, captured by running the engine, not derived from a specification. `ModernPricingEngine` is judged against that table, not against new expectations written during the rebuild:

```cpp
TEST(ModernPricingEngineTest, MatchesEveryCharacterizationCase) {
    ModernPricingEngine engine;
    for (const auto& test_case : pricing_characterization_cases()) {
        EXPECT_EQ(engine.quote(test_case.request).total_cents,
                   test_case.expected_total_cents)
            << "case: " << test_case.name;
    }
}
```

`ModernPricingEngine` restructures the knowledge without discarding it: a named `zone_rate_card()` for the inline matrix, a `CustomerAdjustment` struct and lookup table for the `if`/`else` chain, and the quarter-dollar rounding kept. The rewrite improves the shape of the code; it doesn't claim the pricing rules were wrong. Both engines are also run through the same table and compared directly, independent of either one's pinned totals.

`StranglerPricingRouter` is the seam. `Mode::Compare` runs both engines on every request, records any disagreement and still returns the legacy answer:

```cpp
case Mode::Compare: {
    const Quote legacy_quote = legacy_->quote(request);
    const Quote modern_quote = modern_->quote(request);
    if (legacy_quote.total_cents != modern_quote.total_cents) {
        disagreements_.push_back(QuoteDisagreement{request, legacy_quote, modern_quote});
    }
    return legacy_quote;
}
```

With a stand-in modern engine built to disagree, the router records the disagreement and still returns the legacy total, so a defect introduced in the rewrite can't reach a customer during verification. Once the real `ModernPricingEngine` shows zero disagreements across the table, `CutoverPolicy` moves named customers over one at a time in `Mode::Routed`:

```cpp
EngineChoice CutoverPolicy::choose(const ShipmentRequest& request) const {
    return is_migrated(request.customer_id) ? EngineChoice::Modern : EngineChoice::Legacy;
}
```

The demo runs five releases: `LegacyOnly`; `Compare`, which finds zero disagreements; `Routed` with one customer migrated; `Routed` with every customer migrated; and `ModernOnly`, where `LegacyPricingEngine` could be deleted. Every quote is identical across all five. The migration is invisible to customers because the table and the compare mode showed it would be before a real request reached the new engine.

## A stable interface's failure path: C++20 versus C++23

`PricingEngine::quote()` throws `std::invalid_argument` for a negative weight or an unknown zone pair, and every engine honors that contract under both standards. A C++20 caller that wants a result instead of an exception catches it directly, or wraps the call in a helper that reports failure through a sentinel and loses the reason:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: a caller-side helper loses the reason for rejection; it learns
// *that* a request was invalid, not *why*, unless it also catches the
// exception directly.
std::optional<strangler30::Quote> try_quote_cpp20(const strangler30::PricingEngine& engine,
                                                   const strangler30::ShipmentRequest& request) {
    try {
        return engine.quote(request);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}
```
```cpp
// C++23: PricingEngine::try_quote() is implemented once, in the base
// class, and returns a typed error every caller can branch on without
// touching an exception at the call site.
std::expected<Quote, PricingError> try_quote(const ShipmentRequest& request) const {
    if (request.weight_kg < 0.0) {
        return std::unexpected(PricingError::NegativeWeight);
    }
    try {
        return quote(request);
    } catch (const std::invalid_argument&) {
        return std::unexpected(PricingError::UnknownZonePair);
    }
}
```

Neither engine knows `std::expected` exists. `try_quote()` is written once in `PricingEngine` using the non-virtual interface idiom and calls the `quote()` override every engine already has. The caller-facing contract gained a typed failure path, and neither the legacy engine nor its replacement changed.

> **🤖 Explore with your agent**
>
> "In `examples/30-refactor-or-rewrite`, write `ModernPricingEngine` implementing `PricingEngine::quote()`. Keep every rule `LegacyPricingEngine` encodes: the 15-lane zone matrix as a named `zone_rate_card()`, ACME-2019's $5.00 off subtotals above $20.00, GLOBEX's 10% off, and rounding every total up to the nearest quarter dollar. Replace the customer `if`/`else` chain with a `CustomerAdjustment` lookup table. Don't change `LegacyPricingEngine` or the expected totals in `tests/pricing_characterization_cases.hpp`; the new engine must match all seven cases. Then put both behind `StranglerPricingRouter` and confirm `Mode::Compare` records zero disagreements."
>
> Check with `ctest --preset cpp23 -R 'ModernPricingEngine|Agreement|Strangler'`, and reject a reply that edits an expected total to make a case pass.

## Build, run, observe

From the repository root, the superbuild picks up `examples/30-refactor-or-rewrite`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 tests pass under `cpp23` and all 12 under `cpp20`; the difference is `PricingEngine::try_quote()`, which exists only under C++23. To watch the five-release cutover:

```sh
examples/30-refactor-or-rewrite/demo.sh cpp23
```

```
== Release 1: LegacyOnly, nothing has moved yet ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75

== Release 2: Compare mode, modern runs silently alongside legacy ==
disagreements found: 0 (0 means modern is safe to start taking traffic)

== Release 3: Routed, ACME-2019 migrated, GLOBEX and WIDGETCO still legacy ==
ACME-2019 quote (now via modern): $16.00
GLOBEX quote (still via legacy): $5.00
customers migrated so far: 1 of 3

== Release 4: Routed, every customer migrated ==
GLOBEX quote (now via modern): $5.00
WIDGETCO quote (now via modern): $68.75
customers migrated so far: 3 of 3

== Release 5: ModernOnly, LegacyPricingEngine can be deleted ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75
```

Every total is the same from the first release to the last: the cutover is invisible to a paying customer. The `cpp20` demo prints the same output.

## When not to use it

The strangler fig has a cost, and the same factors that justify it argue against it when circumstances change.

| Situation | Do this instead |
| --- | --- |
| Low risk, low cost, well understood; a bug would be caught in review and fixed in an afternoon | Build the replacement, run it against the characterization table once, and cut over directly |
| No way to tell one customer's request from another's where routing happens | Run compare mode until confident, then switch everyone at once |
| Fifty lines, already tested, understood by everyone who touches it | Refactor in place; a `PricingEngine`-style interface for two coexisting implementations is speculative generality |
| No characterization tests yet | Build them first; a router with nothing pinned gives the new engine nothing to agree with |

`CutoverPolicy` works here because every `ShipmentRequest` carries a `customer_id` to key on. Without that, the gradual cutover collapses to a single switch, which is a legitimate simplification. In every case, the decision factors come first: settle risk, cost, undocumented knowledge and the safety net, and let those answers decide whether the router is worth building.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the definition of legacy code as code without tests, the catalogue of seams and dependency-breaking techniques, and the case for characterization tests before any change.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the refactoring catalogue and the discipline of small, behavior-preserving steps behind the "usually refactor" default.
- Martin Fowler, ["StranglerFigApplication"](https://martinfowler.com/bliki/StranglerFigApplication.html): the original description of the strangler fig technique, including the naming history behind the metaphor.
- The full reading list is on the [bibliography page]({{ '/bibliography/' | relative_url }}).
