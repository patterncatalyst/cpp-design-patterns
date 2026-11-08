---
title: "Programming principles, reviewed in code"
order: 38
layout: tutorial
sectionid: docs
part: appendices
appendix: "F"
description: "A code-first review of the wider programming-principle vocabulary beyond SOLID: the names an engineer is expected to know, each grounded in a before-and-after shipping-domain C++ snippet and a note on where the principle costs something."
---

## Learning objectives

By the end of this appendix you can:

- Tell a principle (the force) from a pattern (one structure that answers it).
- Recognize a violation of each principle in shipping-domain C++ and write the fix.
- Say where each principle costs something and when to stop applying it.

## Why this matters

A principle names a force in a form general enough to apply with or without a pattern nearby. A pattern is one structure that answers it. DRY is a why; extracting a shared `shipment_total_cents()` is a how. Encapsulate What Changes is a why; Strategy is one how.

SOLID is taught in full in [SOLID I]({{ '/docs/03-solid-first-principles/' | relative_url }}) and [SOLID II]({{ '/docs/04-solid-dependency-inversion/' | relative_url }}) and only recapped here. Each other entry gives the principle, a violation, the fix, and the tension: every principle has a counter-force, and knowing its name isn't knowing when to stop.

{% include excalidraw.html name="38-principle-families" caption="Figure F.1: The programming-principle vocabulary grouped into six families, from a tight SOLID recap through a process family with no code shape at all." %}

## Substitutability and responsibility (SOLID, recapped)

| Principle | Statement | In the SOLID examples |
| --- | --- | --- |
| Single Responsibility | A class has one reason to change. | A billing engine splits into `InvoiceCalculator`, `InvoiceFormatter` and `InMemoryInvoiceRepository`, answering to a tax-rate update, a receipt wording change and a storage migration. |
| Open/Closed | Open to new behavior, closed against edits to working behavior. | `FlatServiceFeeRule` arrives as one new `InvoiceRule`; `InvoiceCalculator::totals_for()` and the older rules don't change. |
| Liskov Substitution | A subtype honors the base's whole contract, not just its signatures. | `PercentageDiscountRule` clamps the rate in a `final` method, so `LoyaltyDiscountRule`, which overrides only the protected rate hook, can't discount past the subtotal. |
| Interface Segregation | No client depends on a capability it never uses. | A fat `NotificationGateway` splits into `MessageSender`, `TemplateRenderer` and `DeliveryStatusProvider`; `SmsGateway` implements only its one role. |
| Dependency Inversion | Policy and detail both depend on an abstraction. | `OrderNotificationPolicy` holds a `MessageSender&`, never an `EmailGateway`, so it links and tests without naming a channel. |

None of the five is free. Each pays off once a second case, rule or implementation is in hand or clearly coming.

## Simplicity and scope

### KISS (Keep It Simple)

Fewer moving parts are cheaper to verify and to change.

```cpp
// Violates KISS: a rule-engine abstraction built for a carrier that
// currently charges exactly one surcharge.
class SurchargeRule {
public:
    virtual ~SurchargeRule() = default;
    virtual Cents apply(Cents base_rate_cents) const = 0;
};

class FuelSurchargeRule : public SurchargeRule {
public:
    Cents apply(Cents base_rate_cents) const override {
        return base_rate_cents / 20;
    }
};
```

```cpp
// Honors KISS: the one surcharge this carrier charges, expressed as
// the one function it needs.
Cents fuel_surcharge_cents(Cents base_rate_cents) {
    return base_rate_cents / 20;  // flat 5%
}
```

**Tension:** once a second surcharge arrives, KISS yields to Open/Closed. Past that point the simple function grows a chain of `if` statements, a worse kind of complexity.

### YAGNI (You Aren't Gonna Need It)

Code for a need that hasn't arrived costs as much to read and maintain as code for one that has, and nothing exercises it.

```cpp
// Violates YAGNI: fields added for a multi-currency, multi-leg
// routing feature no carrier integration in this system has built.
struct RateQuoteRequest {
    double weight_kg;
    DestinationZone zone;
    std::string currency_code = "USD";       // never set to anything else
    std::vector<std::string> routing_legs;    // always empty
};
```

```cpp
// Honors YAGNI: only the fields a real carrier integration consumes
// today.
struct RateQuoteRequest {
    double weight_kg;
    DestinationZone zone;
};
```

**Tension:** YAGNI doesn't excuse skipping an extension point a second carrier integration needs next sprint. The question is whether the need is clearly coming, not whether it's imaginable.

### Do The Simplest Thing That Could Possibly Work

Write the plain code that passes the failing test, and promote it when a second case shows up. This is about implementation order; KISS is about the shape you land on.

```cpp
// Skips the simplest thing: a Strategy hierarchy built for a
// tracking lookup that today has exactly one data source.
class TrackingStatusSource {
public:
    virtual ~TrackingStatusSource() = default;
    virtual std::string status_for(std::string_view tracking_id) const = 0;
};
```

```cpp
// Does the simplest thing that could possibly work: a direct lookup.
// Promote this to an interface once a second tracking source exists.
std::string status_for(std::string_view tracking_id, const CarrierClient& client) {
    return client.lookup(tracking_id);
}
```

**Tension:** "simplest" is a start, not a place to stay once a second carrier's tracking API appears. [Strategy]({{ '/docs/08-strategy/' | relative_url }}) covers the same pattern-or-plain-code judgment.

### Avoid Premature Optimization

Spend effort on a bottleneck a profiler has shown you. A guessed bottleneck is usually wrong, and the code built to chase it costs more to maintain than it buys.

```cpp
// Premature: a hand-rolled freelist for Quote objects, written
// before any profiler has shown quote construction costs anything.
class QuoteFreelist {
public:
    Quote* acquire();
    void release(Quote* quote);
private:
    std::array<Quote, 256> storage_;
    std::bitset<256> in_use_;
};
```

```cpp
// Measured first: a plain vector. reserve() avoids reallocation for
// the batch sizes this quoting run sees, nothing more.
std::vector<Quote> quotes;
quotes.reserve(expected_shipment_count);
```

**Tension:** it cuts both ways. Once a profiler names quote construction as a hot loop under real load, declining to optimize is its own mistake. Measure, then decide.

### Curly's Law (Do One Thing)

A function that does one thing is easier to name, test alone and reuse.

```cpp
// Violates Curly's Law: validating, pricing, and logging in one
// function; none of the three steps can be reused or tested alone.
Cents quote_and_log_shipment(const Shipment& shipment, Logger& logger) {
    if (shipment.weight_kg <= 0.0) throw std::invalid_argument("bad weight");
    const Cents rate = shipment.weight_kg > 20.0 ? 4'500 : 1'200;
    logger.info("quoted {} cents", rate);
    return rate;
}
```

```cpp
// Honors Curly's Law: validate, price, and log are three functions;
// the caller composes them, and each is independently testable.
void validate_shipment(const Shipment& shipment) {
    if (shipment.weight_kg <= 0.0) throw std::invalid_argument("bad weight");
}

Cents rate_for(const Shipment& shipment) {
    return shipment.weight_kg > 20.0 ? 4'500 : 1'200;
}

void log_quote(Cents rate, Logger& logger) {
    logger.info("quoted {} cents", rate);
}
```

**Tension:** splitting past the point where anyone would call a piece on its own gives a maze of one-line functions, the over-segregation failure Interface Segregation warns about.

## Structure and change

### DRY (Don't Repeat Yourself)

One fact lives in one place, so fixing it once fixes it everywhere. The target is duplicated knowledge, a business rule written twice, not text that happens to look alike.

```cpp
// Violates DRY: the same subtotal-plus-fuel-surcharge formula
// written once for the quote preview and again for the invoice.
Cents preview_total(const Shipment& shipment) {
    const Cents base = rate_for(shipment);
    return base + base / 20;
}

Cents invoice_total(const Shipment& shipment) {
    const Cents base = rate_for(shipment);
    return base + base / 20;
}
```

```cpp
// Honors DRY: one function both call; a change to the surcharge
// formula has exactly one place to go wrong.
Cents shipment_total_cents(const Shipment& shipment) {
    const Cents base = rate_for(shipment);
    return base + base / 20;
}
```

**Tension:** two functions that both add two numbers but encode unrelated rules stay separate. Merged under DRY, a change to one rule silently changes the other.

### Separation of Concerns

Parsing, pricing and storage live in separate modules, so a change to one doesn't ripple into the others. Single Responsibility says the same at the scale of one class; this says it for a module or layer.

```cpp
// Violates Separation of Concerns: parsing, pricing, and persistence
// in one handler.
void handle_quote_request(const HttpRequest& request, Database& db) {
    const auto shipment = parse_shipment_json(request.body);
    const Cents rate = rate_for(shipment);
    db.exec("INSERT INTO quotes (weight, rate) VALUES (?, ?)", shipment.weight_kg, rate);
}
```

```cpp
// Honors Separation of Concerns: each concern is a module with one
// job; the handler only sequences them.
void handle_quote_request(const HttpRequest& request, Database& db) {
    const Shipment shipment = ShipmentParser::parse(request.body);
    const Cents rate = RateCalculator::rate_for(shipment);
    QuoteRepository(db).save(shipment, rate);
}
```

**Tension:** every boundary has a cost for the hop across it. Split where concerns change on different schedules, not wherever a boundary could be drawn.

### Encapsulate What Changes

Put the part expected to vary behind a stable interface, so the stable part is never touched when it varies. Strategy answers this force in full.

```cpp
// Violates the principle: the varying part (how to price) sits
// inline inside the thing that should never vary (the checkout flow).
Cents checkout_total(const Shipment& shipment, std::string_view pricing_model) {
    if (pricing_model == "flat") return 1'200;
    if (pricing_model == "by_weight") return static_cast<Cents>(shipment.weight_kg * 150);
    throw std::invalid_argument("unknown pricing model");
}
```

```cpp
// Honors the principle: the varying part moves behind an interface;
// checkout_total never changes again when a third model arrives.
Cents checkout_total(const Shipment& shipment, const RateStrategy& strategy) {
    return strategy.rate_for(shipment);
}
```

**Tension:** add the interface when the second pricing model is in hand. An interface with one implementation pays the cost with none of the benefit yet.

### Hide Implementation Details

Callers depend on what a type does, not how it stores its data, so the representation can change without touching them.

```cpp
// Violates the principle: callers reach into the raw event list, and
// each one re-derives "the latest status" slightly differently.
struct TrackingRecord {
    std::vector<TrackingEvent> events;  // public, order not guaranteed
};
```

```cpp
// Honors the principle: the representation is private; every caller
// asks the one question the type answers correctly.
class TrackingRecord {
public:
    void record_event(TrackingEvent event);
    std::string_view latest_status() const;
private:
    std::vector<TrackingEvent> events_;
};
```

**Tension:** hiding a representation every caller legitimately needs, such as iterating all events for an audit, adds a wrapper per field. Hide what varies; expose what callers compute from.

### Composition Over Inheritance

Assemble an object from small collaborators instead of inheriting purely to reuse behavior. A real is-a relationship with polymorphic substitution is Liskov's territory, and inheritance is right there.

```cpp
// Violates the principle: inheritance reused for convenience, not
// for an is-a relationship, reaching into the base's internals.
class PriorityTrackingRecord : public TrackingRecord {
public:
    void record_priority_event(TrackingEvent event) {
        events_.push_back(std::move(event));  // reaches into base state
        is_priority_ = true;
    }
private:
    bool is_priority_ = false;
};
```

```cpp
// Honors the principle: PriorityTracker composes a TrackingRecord
// instead of extending it, and depends only on its public interface.
class PriorityTracker {
public:
    void record_priority_event(TrackingEvent event) {
        record_.record_event(std::move(event));
        is_priority_ = true;
    }
private:
    TrackingRecord record_;
    bool is_priority_ = false;
};
```

**Tension:** composition that forwards every method of the composed type is hand-written inheritance with more boilerplate and no type relationship. Compose when the ask is reuse, not substitutability.

### Orthogonality

Changing one part never forces a change in an unrelated part through a hidden side effect.

```cpp
// Violates orthogonality: submitting a label flips an unrelated
// global logging setting, so switching carriers silently changes how
// tracking events are logged elsewhere in the system.
LabelReceipt FastShipGateway::submit_label_request(const LabelRequest& request) {
    g_log_format = LogFormat::Compact;  // unrelated global, changed here
    return client_.submit(request);
}
```

```cpp
// Honors orthogonality: submitting a label touches only label
// submission; log formatting is configured once, independently.
LabelReceipt FastShipGateway::submit_label_request(const LabelRequest& request) {
    return client_.submit(request);
}
```

**Tension:** past diminishing returns, parts become so decoupled that a real cross-cutting concern, such as tracing one request across label submission and logging, needs its own plumbing to reconnect them.

## Coupling and cohesion

### Minimize Coupling

Fewer, narrower dependencies mean a change ripples into fewer places. Coupling can't reach zero; the question is whether the agreement between modules is a narrow, published interface or a wide, accidental one.

```cpp
// High coupling: reaches past CarrierGateway's interface into its
// internal retry queue to answer one question.
bool has_pending_shipments(const CarrierGateway& gateway) {
    return !gateway.internal_retry_queue().empty();
}
```

```cpp
// Lower coupling: depends on one narrow method CarrierGateway
// publishes; the retry queue stays an implementation detail.
bool has_pending_shipments(const CarrierGateway& gateway) {
    return gateway.has_pending_requests();
}
```

**Tension:** passing primitives instead of a small named type ("stringly typed" calls) trades coupling to a type for coupling to an unenforced convention every call site must get right.

### Maximize Cohesion

Put data next to the operations that use only that data.

```cpp
// Low cohesion: logic that only ever uses Shipment's own fields
// lives far from the struct, in an unrelated utility file.
struct Shipment { double weight_kg; DestinationZone zone; };

Cents compute_base_rate(const Shipment& shipment) {
    return shipment.weight_kg > 20.0 ? 4'500 : 1'200;
}
```

```cpp
// High cohesion: the rate math depends solely on Shipment's own
// fields, so it moves onto Shipment itself.
struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents base_rate() const { return weight_kg > 20.0 ? 4'500 : 1'200; }
};
```

**Tension:** moving every function that takes a `Shipment` onto the class, including logic that leans on unrelated types, builds a god object. Keep on the type only logic that depends solely on its own fields.

### Law of Demeter

A method talks to itself, its parameters, objects it creates and its direct components, not to a stranger's stranger three hops away.

```cpp
// Violates the Law of Demeter: a chain reaching through three
// strangers, coupling the caller to the full shape of three types.
std::string postal = invoice.customer().account().billing_address().postal_code();
```

```cpp
// Honors the Law of Demeter: Invoice exposes the one fact a caller
// needs; the chain still exists, but only Invoice walks it.
class Invoice {
public:
    std::string billing_postal_code() const {
        return customer_.account().billing_address().postal_code();
    }
private:
    Customer customer_;
};
```

**Tension:** wrapping every possible chain bloats the near object with pass-through methods. Apply it to chains outside callers walk.

### Command-Query Separation

A method either does something or answers something, never both, so a query can be called without changing state.

```cpp
// Violates Command-Query Separation: one method reports a value and
// mutates a cursor, so calling it twice to log it gives two answers.
TrackingEvent next_event() {
    TrackingEvent event = events_[cursor_];
    ++cursor_;  // side effect hidden inside what reads like a query
    return event;
}
```

```cpp
// Honors Command-Query Separation: a query that never mutates, and a
// command that never returns a meaningful value.
TrackingEvent peek_next_event() const { return events_[cursor_]; }
void advance_cursor() { ++cursor_; }
```

**Tension:** an atomic pop-and-return on a queue shared across threads needs both in one call under one lock. A documented command that also returns is the usual exception.

### Inversion of Control

A framework, feed or caller decides when your code runs, instead of your code polling. Dependency Inversion is the narrower SOLID claim about which abstraction the dependency points at.

```cpp
// In control: RateCalculator polls the feed on its own schedule,
// deciding when to look for an update.
void poll_for_rate_updates(RateFeed& feed, RateCalculator& calculator) {
    while (running) {
        if (feed.has_update()) calculator.apply_update(feed.latest());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

```cpp
// Control inverted: the feed calls RateCalculator exactly when an
// update exists, and RateCalculator never polls anything.
feed.on_rate_update([&calculator](const RateUpdate& update) {
    calculator.apply_update(update);
});
```

**Tension:** inverting control isn't inverting a dependency. The callback can still name a concrete `RateFeed` and violate Dependency Inversion while correctly inverting who calls whom.

## Interoperability and resilience

### Robustness Principle (Postel's Law)

Be conservative in what you send and liberal in what you accept. Jon Postel stated it for TCP. At a boundary you don't control, such as a carrier's tracking webhook, tolerate variation coming in and introduce none going out.

```cpp
// Violates the Robustness Principle: throws on any field the schema
// does not explicitly list, so a carrier adding an undocumented field
// breaks every shipment update until the parser is patched.
TrackingUpdate parse_webhook(const JsonValue& body) {
    for (const auto& [key, unused_value] : body.items()) {
        if (!is_known_field(key)) throw std::runtime_error("unexpected field: " + key);
    }
    return TrackingUpdate{body["status"].as_string(), body["timestamp"].as_string()};
}
```

```cpp
// Honors the Robustness Principle: unknown fields are ignored, and a
// missing optional field falls back to a documented default.
TrackingUpdate parse_webhook(const JsonValue& body) {
    return TrackingUpdate{
        body.value("status", "unknown"),
        body.value("timestamp", current_timestamp_iso8601()),
    };
}
```

**Tension:** tolerated without limit, it masks a producer bug that should raise an alert. Log what was ignored, or a real data problem looks like a clean parse.

## Principles for teams and process

These have no code shape; they describe how people and schedules behave around code.

| Principle | Statement | In practice | Tension |
| --- | --- | --- | --- |
| Boy-Scout Rule | Leave the code you touch a little cleaner than you found it. | Rename the misleading variable in the function you're already editing; delete the dead comment beside the line you fix. Quality rises through small changes instead of a cleanup project that rarely gets scheduled. | Cleanup folded into an unrelated bug fix buries the fix. Keep it small, in its own commit. |
| Code For The Maintainer | Assume the next reader lacks your current context; often it's you in six months. | Name a variable for what it represents, not its type; comment why a check exists, not what the line says. | Writing for a careless reader produces comments that repeat the code and checks for impossible conditions. Write for a competent engineer new to this change. |
| Murphy's Law | Whatever can go wrong will, usually under load at the worst time. | A carrier API times out, a partition lands during the deploy that mattered, a promised field arrives empty. Concentrate defenses at boundaries: a retrying carrier gateway, a validating invoice intake. | Defending every internal call site duplicates checks. Defend where the uncertainty enters. |
| Brooks's Law | Adding people to a late project makes it later (Fred Brooks, from IBM's OS/360). | Newcomers need ramp-up time from people already stretched, and communication paths grow faster than headcount. Doubling a late tracking migration's team mid-sprint doesn't rescue it. | It covers late projects. Growth with slack, well ahead of a deadline, is a different case. |
| Linus's Law | Given enough eyeballs, all bugs are shallow (Eric S. Raymond, after Linus Torvalds). | A second reviewer on a carrier-integration pull request catches the retry logic the author was too close to see. | It assumes independent context. Ten reviewers who don't know the billing domain multiply rubber stamps, not scrutiny. |

## For deeper coverage

- David Thomas, Andrew Hunt, [*The Pragmatic Programmer: Your Journey to Mastery*](https://pragprog.com/titles/tpp20/the-pragmatic-programmer-20th-anniversary-edition/) (20th Anniversary Edition, Addison-Wesley / Pragmatic Bookshelf): the source for DRY, orthogonality, and the Robustness Principle reviewed in this appendix.
- Robert C. Martin, [*Clean Code: A Handbook of Agile Software Craftsmanship*](https://www.pearson.com/en-us/subject-catalog/p/clean-code-a-handbook-of-agile-software-craftsmanship/P200000009044/9780132350884) (Prentice Hall): the source for the Boy-Scout Rule, Code For The Maintainer, and the do-one-thing discipline behind Curly's Law.
- Frederick P. Brooks Jr., [*The Mythical Man-Month: Essays on Software Engineering*](https://www.pearson.com/en-us/subject-catalog/p/Brooks-Mythical-Man-Month-Anniversary-Edition-The-Essays-On-Software-Engineering-2nd-Edition/P200000000149/9780132119160) (Anniversary Edition, Addison-Wesley): the source for Brooks's Law.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the five SOLID principles the first family above recaps, taught in full in SOLID I and SOLID II.
- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Gang of Four's own advice to favor composition over inheritance and to program to an interface, the root of several principles in the structure-and-change family.
