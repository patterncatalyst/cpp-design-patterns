---
title: "Factory Method and Abstract Factory"
order: 12
part: "creating-objects-under-control"
description: "Two idiomatic ways to keep object creation out of client code, taught against a carrier integration domain: an Abstract Factory that hands out a matching family of label formatter, rate quoter, and tracking client per carrier, and a Factory Method that lets an onboarding environment decide which integration test suite runs inside a fixed sequence."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force both patterns answer: client code that needs an object without naming the concrete class that produces it.
- Build an Abstract Factory that hands out a family of related products per carrier, and explain why one factory keeps the family consistent.
- Build a Factory Method as a private virtual hook inside a fixed creator sequence, and tell it from Abstract Factory by what each varies: one product versus a whole family.
- Choose between a lookup that throws on an unknown name and one that returns `std::expected`, new in C++23.
- Recognize when either pattern does what a constructor call already does, and say what to write instead.

## Why this matters

Handing a shipment to a carrier takes three things that must agree: a label in that carrier's format, a rate quote from that carrier's pricing rule, and a tracking URL on that carrier's domain. A label from one carrier with a quote from another is a box the warehouse can't scan, billed by a formula that doesn't match the label. Checkout shouldn't know which carrier it's talking to, and the three objects it gets must come from the same carrier every time.

A narrower version of the same problem appears in carrier onboarding. Before an account ships live traffic it passes an integration test suite, and which suite runs depends on whether the account is sandbox or production. The rest of the workflow never changes.

{% include excalidraw.html name="12-abstract-factory-families" caption="Figure 12.1: CarrierIntegrationFactory declares a family of three abstract products; FastShipIntegrationFactory and SummitFreightIntegrationFactory each implement and create their own matching family, and ShipmentProcessor depends only on the abstractions." %}

## Pattern: Abstract Factory

### The problem

A `Shipment` carries the facts every carrier needs:

```cpp
struct Shipment {
    std::string destination_country;
    double weight_kg;
    Cents declared_value_cents;
};
```

FastShip charges a flat base fee plus a per-kilogram rate. SummitFreight charges by weight plus a declared-value tier and prints a different label format. If checkout constructs each carrier's label formatter, rate quoter and tracking client itself, it names both carriers, and nothing stops one code path from pairing FastShip's label with SummitFreight's quote.

### The solution

An **Abstract Factory** provides one interface for creating a family of related objects, so a client gets a matching set without naming any concrete class.

### How C++ models it

Three abstract products describe what checkout needs, with no carrier named:

```cpp
class LabelFormatter {
public:
    virtual ~LabelFormatter() = default;
    virtual std::string format_label(const Shipment& shipment) const = 0;
};

class RateQuoter {
public:
    virtual ~RateQuoter() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};

class TrackingClient {
public:
    virtual ~TrackingClient() = default;
    virtual std::string tracking_url(const std::string& tracking_number) const = 0;
};
```

`CarrierIntegrationFactory` declares one creation method per product. Each implementation promises that all three products it returns belong to the same carrier:

```cpp
class CarrierIntegrationFactory {
public:
    virtual ~CarrierIntegrationFactory() = default;
    virtual std::unique_ptr<LabelFormatter> make_label_formatter() const = 0;
    virtual std::unique_ptr<RateQuoter> make_rate_quoter() const = 0;
    virtual std::unique_ptr<TrackingClient> make_tracking_client() const = 0;
};
```

| Factory | Label | Tracking URL domain |
| --- | --- | --- |
| `FastShipIntegrationFactory` | `FASTSHIP-LABEL` lines | `track.fastship.example` |
| `SummitFreightIntegrationFactory` | `SF-MANIFEST` lines | `tracking.summitfreight.example` |

Each factory passes its own account id into all three of its products, so no product can carry another carrier's account.

### The client depends only on abstractions

`ShipmentProcessor` uses a carrier's three products:

```cpp
class ShipmentProcessor {
public:
    explicit ShipmentProcessor(const CarrierIntegrationFactory& factory);
    ProcessedShipment process(const Shipment& shipment, const std::string& tracking_number) const;

private:
    std::unique_ptr<LabelFormatter> label_formatter_;
    std::unique_ptr<RateQuoter> rate_quoter_;
    std::unique_ptr<TrackingClient> tracking_client_;
};
```

Its constructor builds all three from one factory:

```cpp
ShipmentProcessor::ShipmentProcessor(const CarrierIntegrationFactory& factory)
    : label_formatter_(factory.make_label_formatter()),
      rate_quoter_(factory.make_rate_quoter()),
      tracking_client_(factory.make_tracking_client()) {}
```

That constructor is where the guarantee is made. Nothing in `ShipmentProcessor` names a concrete factory, and `process()` can't reach past the three abstract pointers to ask which carrier made them. No code path builds a `FastShipLabelFormatter` next to a `SummitFreightRateQuoter`. A third carrier needs its own factory and three product classes, and `ShipmentProcessor` doesn't change.

### Choosing the factory by name

Checkout reads the merchant's carrier from configuration and calls `make_carrier_factory()` once per order. A private `lowercase()` helper lets configuration say `"FastShip"` or `"fastship"`. An unknown name throws `std::invalid_argument`, which forces every caller to catch or let the exception escape checkout. `try_make_carrier_factory()`, behind `#if __cplusplus >= 202302L`, returns `std::expected` instead:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on an unrecognized carrier name works, but it forces
// every caller to wrap the lookup in a try block or accept an unhandled
// exception on a checkout path.
std::unique_ptr<CarrierIntegrationFactory> make_carrier_factory(std::string_view carrier_name, std::string account_id) {
    const std::string normalized = lowercase(carrier_name);
    if (normalized == "fastship") {
        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));
    }
    if (normalized == "summitfreight") {
        return std::make_unique<SummitFreightIntegrationFactory>(std::move(account_id));
    }
    throw std::invalid_argument("unknown carrier name: " + std::string(carrier_name));
}
```
```cpp
// C++23: std::expected reports the same failure as a typed error,
// checkable with an if and no exception machinery.
std::expected<std::unique_ptr<CarrierIntegrationFactory>, std::string> try_make_carrier_factory(
    std::string_view carrier_name, std::string account_id) {
    const std::string normalized = lowercase(carrier_name);
    if (normalized == "fastship") {
        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));
    }
    if (normalized == "summitfreight") {
        return std::make_unique<SummitFreightIntegrationFactory>(std::move(account_id));
    }
    return std::unexpected<std::string>("unknown carrier name: " + std::string(carrier_name));
}
```

Both resolve the same names to the same factories. Only how a caller learns about an unknown name changes.

## Pattern: Factory Method

### The problem

`CarrierOnboarder` runs a fixed sequence: validate the account id, run an integration suite, judge pass or fail from its log. Sandbox accounts have no live webhook, so their suite skips the webhook check; production accounts must pass it. An `if` on the environment inside the sequence puts both suites' construction in the workflow and grows with each new environment.

### The solution

A **Factory Method** defers the creation of one product to a subclass, through a method the base class calls inside a sequence it otherwise owns.

{% include excalidraw.html name="12-factory-method-flow" caption="Figure 12.2: onboard() runs a fixed sequence around one varying hook, create_test_suite(), which SandboxOnboarder and ProductionOnboarder each plug a different test suite into." %}

### How C++ models it

`create_test_suite()` is a single private virtual method that returns one product:

```cpp
class CarrierOnboarder {
public:
    virtual ~CarrierOnboarder() = default;
    OnboardingResult onboard(const CarrierAccount& account) const;

private:
    virtual std::unique_ptr<IntegrationTestSuite> create_test_suite() const = 0;
};
```

`onboard()` calls it once, in the middle of the sequence:

```cpp
OnboardingResult CarrierOnboarder::onboard(const CarrierAccount& account) const {
    if (account.account_id.empty()) {
        throw std::invalid_argument("carrier account id must not be empty");
    }

    const std::unique_ptr<IntegrationTestSuite> suite = create_test_suite();
    std::vector<std::string> log = suite->run(account);

    const bool passed = std::none_of(log.begin(), log.end(), [](const std::string& line) {
        return line.find("FAILED") != std::string::npos;
    });

    return OnboardingResult{passed, std::move(log)};
}
```

`SandboxOnboarder` returns a `SandboxTestSuite`, which skips the webhook check. `ProductionOnboarder` returns a `ProductionSmokeTestSuite`, which fails an account with no registered webhook. Neither touches `onboard()`. An account with an empty id throws `std::invalid_argument` before any suite is created.

### One product or a family

| | Abstract Factory | Factory Method |
| --- | --- | --- |
| What varies | A family of related products, together | One product inside a fixed sequence |
| Who owns the sequence | The client | The base class |
| Varied by | Passing a different factory object | Overriding one private virtual method |
| Here | `ShipmentProcessor` needs label, quote and tracking from one carrier | `CarrierOnboarder` needs one suite per environment |

Using Abstract Factory for one varying product leaves creation methods nobody calls. Using Factory Method for three products that must agree makes the caller make three calls and trust they came from the same place, the trust one factory call removes.

## When not to use it

- **One carrier.** With no second carrier coming, construct a `LabelFormatter`, `RateQuoter` and `TrackingClient` directly in `ShipmentProcessor`'s constructor. The full Abstract Factory adds three interfaces, six creation methods and a lookup to prevent mixing when there's nothing to mix.
- **One way to run the step.** A workflow with one test suite needs a function that runs it inline, not a base class, one subclass and one override. Introduce `create_test_suite()` when a second environment needs a different product at that point.
- **The wrong one of the two.** A three-method factory interface where two methods have one caller and one implementation is Abstract Factory solving a Factory Method problem. Identify the force, one varying product in a fixed sequence, and use the narrower pattern.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a factory.** "Add a third carrier" leaves open whether the carrier gets its own factory or a branch in existing code, and whether `ShipmentProcessor` changes. State it:

> In `examples/12-factories`, add a carrier called NorthwindPost. Write `NorthwindPostIntegrationFactory` implementing `CarrierIntegrationFactory`, with its own `LabelFormatter`, `RateQuoter` and `TrackingClient` classes that all use the account id passed to the factory's constructor. Add one `"northwindpost"` branch to `make_carrier_factory()` and the same branch to `try_make_carrier_factory()`, and change nothing else: no carrier name or concrete class may appear in `ShipmentProcessor`. Add a test that the label, quote and tracking URL from one `ShipmentProcessor` all belong to NorthwindPost.

Run `ctest --preset cpp23` and check that `ShipmentProcessor`'s files are unchanged.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/12-factories`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 cases pass under `cpp23` and 12 under `cpp20`; the two `std::expected` cases compile only under C++23. To see both patterns run:

```sh
examples/12-factories/demo.sh cpp23
```

It processes a 4.2 kg shipment through both carrier families, looks a factory up by name, onboards one account in both environments and rejects an unknown carrier:

```
== Abstract Factory: two carrier families, one client ==
FastShip:
  label:   FASTSHIP-LABEL acct=acct-fastship-77 dest=CA wt=4.2kg
  rate:    1106 cents
  tracking: https://track.fastship.example/acct-fastship-77/TRK-1001
SummitFreight:
  label:   SF-MANIFEST account:acct-summit-42 to:CA weight:4.2
  rate:    1508 cents
  tracking: https://tracking.summitfreight.example/shipments/acct-summit-42-TRK-2002

== Abstract Factory: carrier chosen by name at startup ==
Looked up ("fastship"):
  label:   FASTSHIP-LABEL acct=acct-fastship-77 dest=CA wt=4.2kg
  rate:    1106 cents
  tracking: https://track.fastship.example/acct-fastship-77/TRK-3003

== Factory Method: environment decides the test suite ==
Sandbox onboarding (PASSED):
  sandbox: validated account id acct-fastship-77
  sandbox: accepted a synthetic test shipment
  sandbox: skipped webhook delivery check (not required in sandbox)
Production onboarding (PASSED):
  production: validated account id acct-fastship-77
  production: quoted a live test shipment
  production: confirmed webhook delivery

== C++23 refinement: typed error for an unknown carrier ==
Rejected unknown carrier: unknown carrier name: globex-shipping
```

The factory looked up as `"fastship"` quotes the same 1106 cents and builds the same tracking URL pattern as the directly constructed `FastShipIntegrationFactory`. Under `cpp20` the output stops before the `std::expected` block.

- **Mix one product into the wrong family.** In a scratch copy, make `FastShipIntegrationFactory::make_tracking_client()` return a `SummitFreightTrackingClient`. It compiles, and two cases fail: the FastShip family is no longer consistent, and a `ShipmentProcessor` built from it prints a SummitFreight tracking URL.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Factory Method and Abstract Factory entries, including the "family of related or dependent objects" framing the two carriers build on.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/12-factories`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
