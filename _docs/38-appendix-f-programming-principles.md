---
title: "Programming principles, reviewed in code"
order: 38
layout: tutorial
sectionid: docs
part: appendices
appendix: "F"
description: "A code-first review of the wider programming-principle vocabulary beyond SOLID: the names an engineer is expected to know, each grounded in a before-and-after shipping-domain C++ snippet and a note on where the principle costs something."
---

## How to read this appendix

A principle is force-level guidance that sits beneath a pattern. A pattern names a structure, Strategy, Observer, Decorator, built to answer a specific force; a principle names the force itself, in a form general enough to apply whether or not a named pattern is anywhere nearby. This book teaches the five SOLID principles in full, with a runnable example and a GoogleTest suite proving each one, in the SOLID chapters. This appendix does two things the SOLID chapters do not: it recaps SOLID in a few sentences each, pointing back at the fuller treatment rather than repeating it, and it catalogs the broader vocabulary of principles an engineer is expected to recognize by name, each reviewed against a small before-and-after in this book's shipping domain, carriers, rate quotes, tracking, invoices.

The short way to hold the distinction: a principle says *why*, a pattern is a *how* that honors it. DRY is a why. Extracting a shared `shipment_total_cents()` function is a how. Encapsulate What Changes is a why. Strategy is one how that answers it. Every entry below follows the same shape: why the principle matters, code that violates it, code that honors it, and a tension note, because every principle on this list has a cost or a counter-force, and naming the principle is not the same as knowing when to stop applying it.

{% include excalidraw.html name="38-principle-families" caption="Figure F.1: The programming-principle vocabulary grouped into six families, from a tight SOLID recap through a process family with no code shape at all." %}

## Substitutability and responsibility (SOLID, recapped)

The SOLID chapters build these five principles in full against a billing engine and an order-notification system, with a GoogleTest suite proving each claim directly rather than describing it. What follows is a pointer back to that material, not a restatement of it.

**Single Responsibility.** A class should have exactly one reason to change. The SOLID chapters split a billing engine's calculation, formatting, and persistence into `InvoiceCalculator`, `InvoiceFormatter`, and `InMemoryInvoiceRepository`, each answerable to one of three unrelated schedules of change, a tax-rate update, a receipt wording change, a storage migration.

**Open/Closed.** A module should be open to new behavior and closed against edits to behavior that already works. The `InvoiceRule` abstraction in the SOLID chapters lets a new `FlatServiceFeeRule` arrive as one new file; `InvoiceCalculator::totals_for()` and every rule that predates it stay byte-for-byte unchanged.

**Liskov Substitution.** A subtype must honor its base type's whole contract, not just its method signatures. `PercentageDiscountRule` clamps its discount rate in a `final` method so that `LoyaltyDiscountRule`, which only overrides the protected rate hook, cannot discount an invoice past its own subtotal no matter what rate the hook computes.

**Interface Segregation.** A client should not be forced to depend on a capability it never uses. The SOLID chapters split a fat `NotificationGateway` into `MessageSender`, `TemplateRenderer`, and `DeliveryStatusProvider`, so an `SmsGateway` implements exactly the one role it has.

**Dependency Inversion.** High-level policy and low-level detail should both depend on an abstraction, not on each other. `OrderNotificationPolicy` holds a `MessageSender&`, never an `EmailGateway`, so the policy links and tests without ever naming a concrete channel.

The SOLID chapters are also where "when SOLID works against you" lives in full: none of the five is free, and all five earn their cost only when a second case, a second rule, a second implementation, is in hand or clearly coming.

## Simplicity and scope

### KISS (Keep It Simple)

A design with fewer moving parts is cheaper to verify and cheaper to change later, because there is less structure standing between a reader and the one fact that matters.

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

Tension: once a second surcharge arrives, KISS yields to the Open/Closed force; keeping the function simple past that point means a growing chain of `if` statements, which is a different and worse kind of complexity.

### YAGNI (You Aren't Gonna Need It)

Code built for a need that has not arrived yet costs the same to read, review, and maintain as code answering a need that has, with nothing exercising it to prove it is correct.

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

Tension: YAGNI is not license to skip an extension point a second carrier integration is already scheduled to need next sprint; the test is whether the need is clearly coming, not whether it is merely imaginable.

### Do The Simplest Thing That Could Possibly Work

Write the plain code that passes the test in front of you before reaching for a general structure, and promote it once a second case shows up. This is a discipline about implementation order; KISS is about the shape you land on once you are done.

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

Tension: "simplest" is a starting point, not a place to stay once a second carrier's tracking API shows up; this is the same candidate-pattern-versus-candidate-non-pattern judgment the Strategy pattern trains in full.

### Avoid Premature Optimization

Spend engineering effort on a bottleneck a profiler has shown you, not on a guess about where one might be. The guess is usually wrong, and the code built to chase it usually costs more to maintain than the performance it was meant to buy.

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

Tension: the principle cuts both ways. Once a profiler does name quote construction as a hot loop under real load, declining to optimize out of general caution becomes its own mistake; measure, then decide, in either direction.

### Curly's Law (Do One Thing)

A function or a module that does one thing is easier to name precisely, test in isolation, and reuse somewhere its sibling logic does not belong.

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

Tension: splitting past the point where a reviewer would want to call a piece on its own produces a maze of one-line functions, the same over-segregation failure the SOLID recap above warns about for Interface Segregation.

## Structure and change

### DRY (Don't Repeat Yourself)

One fact should live in exactly one place, so fixing it once fixes it everywhere it is used. The target is duplicated *knowledge*, a business rule written twice, not duplicated *text* that happens to look similar.

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

Tension: two functions that happen to both add two numbers but encode unrelated business rules should stay separate; merging them under DRY's name means a later change to one rule silently changes a rule it was never meant to touch.

### Separation of Concerns

Keep unrelated concerns, parsing a request, pricing a shipment, writing to storage, in separate modules, so a change to one does not ripple into code that has nothing to do with it. Single Responsibility makes the same claim at the scale of one class; this principle makes it at the scale of a module or a layer.

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

Tension: splitting every layer into its own module or service because "concerns are separate" ignores the real cost of the hop between them; apply it where the concerns change on different schedules, not wherever a boundary could theoretically be drawn.

### Encapsulate What Changes

Isolate the part of a design expected to vary behind a stable interface, so the part that never varies never has to be touched when the varying part does. This is the exact force the Strategy pattern answers in full; the principle names the force on its own, with or without a pattern attached to it.

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

Tension: reach for the interface once a second pricing model is in hand, not on the first one; an interface with a single implementation is this principle's cost with none of its benefit yet.

### Hide Implementation Details

A caller should depend on what a type does, not on how it stores the data behind it, so the representation can change without touching every caller.

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

Tension: hiding a representation every caller legitimately needs, iterating all events for an audit, for example, just adds a wrapper method per field; hide what varies, expose what a caller computes.

### Composition Over Inheritance

Prefer assembling an object out of small collaborators to building a deep inheritance tree purely to reuse behavior. Inheritance is still the right choice when a real is-a relationship holds and a caller needs to substitute the derived type polymorphically; that case belongs to Liskov Substitution, not to this one.

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

Tension: composition that forwards every single method of the composed type is reinventing inheritance by hand, with more boilerplate and none of the type relationship; reach for composition when reuse, not substitutability, is what is being asked for.

### Orthogonality

Independent parts of a system should stay independent: changing one should never force an unrelated change somewhere else because of a hidden side effect.

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

Tension: pursuing orthogonality past the point of diminishing returns produces parts so decoupled that a legitimate cross-cutting concern, tracing one request across label submission and logging, needs its own plumbing just to reconnect them.

## Coupling and cohesion

### Minimise Coupling

Fewer, narrower dependencies between modules mean a change in one ripples into fewer places. Coupling cannot reach zero; two collaborating modules must agree on something. The question is whether that agreement is a narrow, published interface or a wide, accidental one.

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

Tension: minimizing coupling by passing primitive values instead of a small, named type ("stringly typed" calls) trades one kind of coupling, to a type, for a worse kind, to an unenforced convention every call site has to get right on its own.

### Maximise Cohesion

Group data with the operations that act only on that data, so a reader finds the logic next to the fields it touches instead of scattered across the codebase.

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

Tension: pulling every function that merely accepts a `Shipment` parameter onto the class itself, including logic that also depends heavily on unrelated types, produces a god object; keep on the type only the logic that depends solely on that type's own fields.

### Law of Demeter

A method should talk to itself, its parameters, objects it creates, and its own direct components, not chain through a stranger's stranger to reach a fact three hops away.

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

Tension: wrapping every possible chain in a delegating method bloats the near object with pass-through methods that exist for no reason but to satisfy the rule; apply it to chains an outside caller walks, not to every getter that could theoretically be chained.

### Command-Query Separation

A method should either do something or answer something, never both, so a caller can read a query call without worrying it also changed state.

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

Tension: strict separation sometimes costs a real feature, an atomic pop-and-return off a queue shared across threads being the common case; some stacks accept a documented command that also returns there rather than pay for two round trips under one lock.

### Inversion of Control

Let a framework, a feed, or a caller decide when your code runs, instead of your code deciding when to go ask. This is the general shape; Dependency Inversion, recapped above, is the specific SOLID claim about which abstraction the dependency points at once control is inverted.

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

Tension: inverting control is not the same as inverting a dependency. The callback above can still name a concrete `RateFeed` type directly and violate Dependency Inversion even while correctly inverting who calls whom.

## Interoperability and resilience

### Robustness Principle (Postel's Law)

Be conservative in what you send, liberal in what you accept. At a boundary where another system's payload is outside your control, a carrier's tracking webhook being the common case, tolerate variation defensively on the way in, and never introduce variation of your own on the way out.

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

Tension: "liberal in what you accept" taken past its limit can mask a real producer bug, silently absorbing a field that should have raised an alert; log what was ignored even while tolerating it, or a real data problem becomes indistinguishable from a clean parse.

## Principles for teams and process

These five are not code-expressible; they describe how people and schedules behave around code, not a shape a compiler checks.

**Boy-Scout Rule.** Leave the code you touch a little cleaner than you found it: rename the misleading variable in the function you are already editing, delete the dead comment next to the line you are fixing. The rule scales a codebase's quality up over many small, incidental improvements instead of waiting for a dedicated cleanup project that rarely gets scheduled. The tension: a boy-scout cleanup folded into an unrelated bug-fix pull request makes the diff harder to review and can bury the actual fix; keep the cleanup small and visible, in its own commit, so a reviewer can tell the fix from the tidying.

**Code For The Maintainer.** Write code assuming the next reader does not have the context sitting in your head right now, and remember that the next reader is often you, six months later, having forgotten exactly this detail. In practice this means naming a variable for what it represents rather than what type it is, and writing a comment that explains why a check exists rather than restating what the line already says. The tension: optimizing purely for an inattentive reader produces comments that repeat the code and defensive checks for conditions that structurally cannot occur, burying the real logic in noise; write for a competent engineer unfamiliar with this specific change, not for a careless one.

**Murphy's Law.** Whatever can go wrong will go wrong, and it tends to happen under load, at the worst time. For a shipping system that means a carrier's API will time out, a network partition will land during the one deploy that mattered, and a field a contract promised would always be present will eventually arrive empty. The practical response is defensive design concentrated at a boundary: a retrying carrier gateway, a validating invoice intake, rather than trusting that a failure mode hasn't happened yet as evidence it never will. The tension: treating every internal call site as if the worst case applies duplicates defensive code everywhere; put the defenses at the boundary where the real uncertainty lives, not on every call downstream of it.

**Brooks's Law.** Adding people to a software project that is already late makes it later, a result Fred Brooks observed directly while running IBM's OS/360 project. New engineers need ramp-up time from people already stretched thin, and the number of communication paths across a team grows faster than the team itself. A shipment-tracking migration running two weeks behind does not get rescued by doubling the engineers assigned to it mid-sprint. The tension: the law describes a late project specifically, not team growth in general; a team with slack, brought on well ahead of a deadline rather than in a crunch, is a different situation the law does not cover.

**Linus's Law.** Given enough eyeballs, all bugs are shallow: a maxim from open-source review culture, associated with Linus Torvalds, claiming that more independent reviewers surface more of a change's defects than one reviewer working alone. Code review on a carrier-integration pull request is this principle applied directly, a second reviewer catching the retry logic a sole author missed because they were too close to it. The tension: the law assumes reviewers bring independent context; ten reviewers skimming the same invoice-calculation diff without understanding the billing domain do not multiply scrutiny, they multiply rubber stamps. The quality of review matters more than the headcount of reviewers.

## For deeper coverage

- David Thomas, Andrew Hunt, [*The Pragmatic Programmer: Your Journey to Mastery*](https://pragprog.com/titles/tpp20/the-pragmatic-programmer-20th-anniversary-edition/) (20th Anniversary Edition, Addison-Wesley / Pragmatic Bookshelf): the source for DRY, orthogonality, and the Robustness Principle reviewed in this appendix.
- Robert C. Martin, [*Clean Code: A Handbook of Agile Software Craftsmanship*](https://www.pearson.com/en-us/subject-catalog/p/clean-code-a-handbook-of-agile-software-craftsmanship/P200000009044/9780132350884) (Prentice Hall): the source for the Boy-Scout Rule, Code For The Maintainer, and the do-one-thing discipline behind Curly's Law.
- Frederick P. Brooks Jr., [*The Mythical Man-Month: Essays on Software Engineering*](https://www.pearson.com/en-us/subject-catalog/p/Brooks-Mythical-Man-Month-Anniversary-Edition-The-Essays-On-Software-Engineering-2nd-Edition/P200000000149/9780132119160) (Anniversary Edition, Addison-Wesley): the source for Brooks's Law.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the five SOLID principles the first family above recaps, taught in full in the SOLID chapters.
- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Gang of Four's own advice to favor composition over inheritance and to program to an interface, the root of several principles in the structure-and-change family.
