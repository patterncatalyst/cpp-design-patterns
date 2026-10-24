---
title: "Factory Method and Abstract Factory"
order: 12
part: "creating-objects-under-control"
description: "Two idiomatic ways to keep object creation out of client code, taught against a carrier integration domain: an Abstract Factory that hands out a matching family of label formatter, rate quoter, and tracking client per carrier, and a Factory Method that lets an onboarding environment decide which integration test suite runs inside a fixed sequence."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force both patterns answer: client code that needs an object
  without needing to know, or name, the concrete class that produces it.
- Build an Abstract Factory that hands out a family of related products per
  carrier, and explain why building the family from one factory call is
  what keeps the family internally consistent.
- Build a Factory Method as a private virtual hook inside a fixed creator
  sequence, and tell it apart from Abstract Factory by what each one
  varies: one product per call versus a whole family per call.
- Choose between a C++20 lookup that throws on an unrecognized name and
  C++23's `std::expected` for the same lookup, and say which standard each
  belongs to.
- Recognize when either pattern is solving a problem a constructor call
  already solves, and say what to write instead.

## Why this matters

A checkout has to hand a shipment to whichever carrier a merchant has under
contract, and handing a shipment to a carrier means producing three
related things that all have to agree with each other: a shipping label in
that carrier's own format, a rate quote computed by that carrier's own
pricing rule, and a tracking URL that resolves on that carrier's own
domain. Mix a label from one carrier with a rate quote from another and
the result is not a smaller bug. It is a shipment a warehouse worker
cannot scan, billed by a formula that does not match the label on the box.

That is the shape Chapter 1 called a force: the checkout code that needs a
label, a quote, and a tracking URL should never have to know which carrier
it is talking to, and whichever three objects it gets have to come from
the same carrier every time. Abstract Factory is the Gang of Four's name
for resolving exactly this force. A single factory call produces every
member of a related family at once, so nothing downstream of that call can
end up holding a mismatched set.

Factory Method answers a narrower version of the same underlying problem:
client code that needs one object without naming its concrete class. A
carrier onboarding workflow needs to run an integration test suite before
an account can ship live traffic, and which suite to run depends on
whether the account is a sandbox or a production account. The workflow
itself, validate the account, run a suite, judge the result, never
changes. Only the one step that decides which suite to instantiate does,
and that decision belongs to a subclass, not to an `if` statement the
workflow would otherwise have to carry.

{% include excalidraw.html name="12-abstract-factory-families" caption="Figure 12.1: CarrierIntegrationFactory declares a family of three abstract products; FastShipIntegrationFactory and SummitFreightIntegrationFactory each implement and create their own matching family, and ShipmentProcessor depends only on the abstractions." %}

## The domain: FastShip and SummitFreight

The example behind this chapter lives in `examples/12-factories`. A
`Shipment` carries a destination country, a weight, and a declared value,
the same three facts a carrier needs regardless of which carrier it is:

```cpp
struct Shipment {
    std::string destination_country;
    double weight_kg;
    Cents declared_value_cents;
};
```

Two carriers back this example, FastShip and SummitFreight, and each one
implements the same three roles with its own rules. FastShip charges a
flat base fee plus a per-kilogram rate; SummitFreight charges by weight
plus a declared-value tier, and prints a different label format entirely.
Neither carrier's pricing or label format matters on its own. What matters
is that `ShipmentProcessor`, the client this chapter builds toward, never
has to know either one exists.

## Abstract Factory: a family that must match

Three abstract product interfaces describe what checkout needs, with no
mention anywhere of FastShip or SummitFreight:

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

`CarrierIntegrationFactory` is the Abstract Factory itself: one interface
declaring one creation method per product above, so a single
implementation of this interface is a promise that all three products it
returns belong to the same carrier.

```cpp
class CarrierIntegrationFactory {
public:
    virtual ~CarrierIntegrationFactory() = default;
    virtual std::unique_ptr<LabelFormatter> make_label_formatter() const = 0;
    virtual std::unique_ptr<RateQuoter> make_rate_quoter() const = 0;
    virtual std::unique_ptr<TrackingClient> make_tracking_client() const = 0;
};
```

`FastShipIntegrationFactory` and `SummitFreightIntegrationFactory`
implement all three methods, each returning that carrier's own concrete
products. `FastShipIntegrationFactory::make_label_formatter()` returns a
`FastShipLabelFormatter` that writes `FASTSHIP-LABEL` lines carrying the
account id passed into the factory's constructor; the matching
`FastShipTrackingClient` builds a URL on `track.fastship.example` using
that same account id. `SummitFreightIntegrationFactory` does the same
against its own `SF-MANIFEST` label format and its own
`tracking.summitfreight.example` domain. Neither factory can accidentally
hand out a product tagged with the wrong carrier's account id, because
each factory's three product classes only ever read the one account id
that factory itself was constructed with.

## The client: depends only on the abstractions

`ShipmentProcessor` is the object that uses a carrier's three products:

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

Its constructor builds all three products from one factory in one call:

```cpp
ShipmentProcessor::ShipmentProcessor(const CarrierIntegrationFactory& factory)
    : label_formatter_(factory.make_label_formatter()),
      rate_quoter_(factory.make_rate_quoter()),
      tracking_client_(factory.make_tracking_client()) {}
```

That constructor is the one place the guarantee gets made. Nothing in
`ShipmentProcessor`'s own code names `FastShipIntegrationFactory` or
`SummitFreightIntegrationFactory`, and nothing in `process()` can reach
past the three abstract pointers to ask which carrier produced them. A
third carrier added next quarter needs its own factory and its own three
product classes, and `ShipmentProcessor` does not change at all, because
it was never written against a concrete carrier in the first place. More
to the point, there is no code path anywhere in this example that
constructs a `FastShipLabelFormatter` next to a `SummitFreightRateQuoter`.
The family comes from one factory call, every time, which is the specific
guarantee Abstract Factory exists to provide and a loose collection of
three independently constructed objects cannot.

`make_carrier_factory()` is the function a checkout calls once per order,
after reading the merchant's chosen carrier out of configuration:

```cpp
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

`lowercase()` is a small, private helper in this same file, so a merchant's
configuration can read `"FastShip"` or `"fastship"` and reach the same
factory either way.

## Factory Method: one product, decided by environment

Factory Method solves a narrower problem than Abstract Factory, and the
carrier onboarding workflow in this same example shows the difference
directly. Before a carrier account can process live shipments, it has to
pass an integration test suite, and which suite runs depends on whether
the account is a sandbox account or a production account. The sequence
that runs the check never changes: validate the account id, run a suite,
decide pass or fail from the suite's own log.

```cpp
class CarrierOnboarder {
public:
    virtual ~CarrierOnboarder() = default;
    OnboardingResult onboard(const CarrierAccount& account) const;

private:
    virtual std::unique_ptr<IntegrationTestSuite> create_test_suite() const = 0;
};
```

`create_test_suite()` is the factory method: a single private virtual
method that returns one product, not a family of three. `onboard()` calls
it once, in the middle of a sequence it otherwise owns outright:

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

`SandboxOnboarder` overrides `create_test_suite()` to return a
`SandboxTestSuite`, which skips the webhook delivery check because a
sandbox account has no live webhook to deliver to. `ProductionOnboarder`
overrides the same method to return a `ProductionSmokeTestSuite`, which
requires the webhook check to pass. Neither subclass touches `onboard()`:
the validation, the sequence, and the pass/fail judgment are inherited
unchanged, and the only thing that varies between a sandbox and a
production onboarding run is which one product `create_test_suite()`
hands back.

That is the test worth applying whenever the two patterns in this
chapter's title seem interchangeable. Abstract Factory varies a whole
family of related products together, and the family is the unit a caller
asks for. Factory Method varies exactly one product inside a sequence that
a base class owns, and the sequence is the thing that stays fixed while a
subclass decides the one part that does not. `ShipmentProcessor` needed a
family; `CarrierOnboarder` needed a single decision point inside an
otherwise unchanging workflow. Reaching for Abstract Factory when only one
product varies adds two unused creation methods to an interface nobody
calls; reaching for Factory Method when three related products need
to agree with each other forces a caller to make three separate calls and
trust that it got them all from the same place, which is exactly the trust
Abstract Factory's single call exists to remove.

> **AI sidebar.** Ask an LLM pairing partner to "add a factory" for a
> second carrier or a second environment, and a common result is a
> `switch` statement or an `if`/`else if` chain inside the one function
> that already does everything else, FastShip's branch, SummitFreight's
> branch, and now a third carrier's branch, all living in the same
> function body the generated code was supposed to extend cleanly. Check
> the result against the distinction this chapter trained: did the model
> add a new concrete factory implementing the existing abstract interface,
> leaving `ShipmentProcessor` and every other factory's code untouched, or
> did it add a branch to an existing function that now has to be read and
> tested all over again because every carrier's logic sits inside it? A
> generated `if (carrier == "newcarrier")` dropped into
> `make_carrier_factory()`'s existing chain is a correct, idiomatic fix for
> that one function. The same branching habit showing up inside
> `ShipmentProcessor` itself, naming a concrete carrier class directly, is
> the sign the model reached past the abstraction this chapter built and
> back toward the thing it was supposed to remove.

## A typed lookup: C++20 versus C++23

`make_carrier_factory()` above throws `std::invalid_argument` on an
unrecognized carrier name, which works but forces every caller either to
wrap the lookup in a `try` block or accept that an unhandled exception can
propagate out of checkout. C++23's `std::expected` gives the same lookup a
typed alternative: a return type that carries either the factory or a
specific reason the lookup failed, checkable with an `if` and no exception
machinery at all. `try_make_carrier_factory()` in this example is guarded
behind `#if __cplusplus >= 202302L` precisely so the mandatory C++20 build
path never needs `std::expected`, while the C++23 build gets the more
precise contract.

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

Both versions resolve the exact same carrier names to the exact same
concrete factories. Only how a caller learns about an unrecognized name
changes between the two columns, the same narrow scope Chapter 8's
codetabs held to for Strategy's error handling.

## When not to use Factory Method or Abstract Factory

Both patterns earn their interface when there is more than one concrete
class in hand, or clearly coming, and when a caller must not name the
concrete type itself. Outside that case, both add a layer with nothing
underneath it.

A single carrier with no second one on any roadmap does not need
`CarrierIntegrationFactory`, three abstract product interfaces, and a
one-implementation hierarchy under each. It needs a `LabelFormatter`,
`RateQuoter`, and `TrackingClient` constructed directly in
`ShipmentProcessor`'s own constructor, with no interface standing between
them and their one caller. Introducing the full Abstract Factory shape
ahead of a second carrier arriving adds three abstract product interfaces,
six creation methods across them, and a lookup function, all defending
against mixed-carrier products that cannot happen yet because there is
nothing to mix.

Factory Method fails the same test in a narrower way. A workflow with
exactly one way to run its one varying step does not need a base class and
a private virtual hook; it needs a function that runs the step inline. The
moment to introduce `create_test_suite()` as a factory method is the
moment a second environment, not a second format of the same thing,
needs a different product at that one point in an otherwise identical
sequence. A single `IntegrationTestSuite` with no sandbox versus
production distinction anywhere in the business rules would make
`CarrierOnboarder`'s hierarchy pure overhead: one base class, one
subclass, one override, standing in for what could be a single function
with no decision to make.

The sharper trap is reaching for Abstract Factory when Factory Method
would do, or the reverse. A three-method interface where two of the three
methods have exactly one caller and no second implementation anywhere in
the codebase is the Abstract Factory shape used to solve a Factory Method
problem: only one product in that family varies, and the other two
methods exist purely because the interface declares them. The fix is not
to delete the interface. It is to identify which force is in play, a
single varying product inside a fixed sequence, and reach for the
narrower pattern that names it.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/12-factories` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset, 12
under `cpp20`, the difference being the two `std::expected`-specific cases
that only compile under C++23. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see both patterns produce real output rather than just reading an
assertion that says they work, run the example's own demo script:

```sh
examples/12-factories/demo.sh cpp23
```

It processes the same 4.2 kg shipment through both carrier families, looks
a carrier factory up by name, runs both onboarding environments against
one account, and finally rejects an unrecognized carrier name:

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

Notice that the carrier looked up by name, `"fastship"`, quotes the
identical $11.06 rate and prints the identical tracking URL as the
directly constructed `FastShipIntegrationFactory` above it. That agreement
is `ShipmentProcessor` depending only on the abstract interfaces showing
up as an observable fact at the terminal: however a `CarrierIntegrationFactory`
reached the processor's constructor, by direct construction or by name
lookup, the family it produces behaves identically. Running
`examples/12-factories/demo.sh cpp20` prints the same output minus the
final `std::expected` block, which only compiles under the `cpp23`
preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design
  Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley):
  the original Factory Method and Abstract Factory entries, including the
  "family of related or dependent objects" framing this chapter's two
  carriers build on.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official
  GoogleTest documentation: the testing idiom behind
  `examples/12-factories`'s suite, which asserts that each factory's
  family stays internally consistent rather than only checking that a
  product constructs without error.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
