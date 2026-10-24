---
title: "Adapter, Facade, Bridge, and PImpl"
order: 15
part: "composing-adapting-structure"
description: "Four structural techniques taught in one cohesive shipping subsystem: an Adapter that wraps a legacy carrier rate API behind this book's own interface, a Facade that fronts rating, labeling, and tracking with one call, a Bridge that lets a notification's kind and its delivery transport vary independently, and the PImpl idiom hiding a label renderer's formatting behind a compilation firewall."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force each of Adapter, Facade, and Bridge resolves, and tell the three apart from each other and from the PImpl idiom they share a part with.
- Build an Adapter that wraps a legacy, differently-shaped rate API behind this book's own `RateSource` interface, by composition rather than inheritance.
- Build a Facade that gives a client one method over several subsystems, and explain what single-call orchestration buys over trusting the client to coordinate three objects correctly, every time, itself.
- Build a Bridge that lets a `Notification`'s kind and its delivery transport vary independently, so refined kinds and transports compose without a class per combination.
- Give a class a PImpl compilation firewall behind `std::unique_ptr<Impl>`, and implement copy and move so the type still behaves like an ordinary value.
- Choose between a C++20 throwing lookup and C++23's `std::expected` for a facade's rejected-input path, and say which standard each belongs to.
- Recognize when each of these four techniques is solving a problem a plain function call or a visible member would already solve.

## Why this matters

A checkout flow needs a shipping quote, a label, and a tracking number, and in practice all three come wrapped in friction the pattern itself did not create. The rate comes from a carrier's own API, built years before this codebase existed, with its own units and its own method names. Producing the quote, the label, and the tracking number is three separate calls that have to happen together, in order, every time, or a bug ships: a label with no matching rate, a tracking number nobody registered. And once a shipment's status changes, the system has to tell someone, by whichever channel that customer prefers, in a message that differs by what happened, not by how the message gets delivered.

None of those three problems is about varying an algorithm, the force Chapter 8 covered, or about producing a family of related objects, the force Chapter 12 covered. They are about structure: how an existing, awkward-shaped class gets a shape the rest of the code can use; how a client gets one thing to call instead of several things to orchestrate; how two things that each vary on their own, a notification's kind and its transport, get decoupled so adding one of either does not force a change to the other. Adapter, Facade, and Bridge are the Gang of Four's names for those three structural forces. PImpl sits alongside them because it answers a related but narrower question, not what an object's public shape should be, but how to keep that shape stable while its private implementation changes underneath it.

{% include excalidraw.html name="15-adapter-facade-bridge" caption="Figure 15.1: Adapter wraps a legacy API behind a target interface; Facade fronts three subsystems with one call; Bridge crosses an abstraction axis of refined notification kinds with an implementation axis of transports." %}

## The domain: a GlobalFreight integration

The example behind this chapter lives in `examples/15-adapter-facade-bridge`, and its domain is one carrier integration, GlobalFreight, carried through all four techniques so each one builds on the last instead of starting over in an unrelated scenario. `Shipment` is the same shape this book has used since Chapter 8: a weight, a destination zone, and a declared value.

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

GlobalFreight is not a class this book gets to redesign. `LegacyGlobalFreightRateApi` is the third-party rate lookup this example wraps, written to GlobalFreight's own conventions: weight travels in grams, the destination travels as a numeric zone code, and the quote comes back as a plain `long`.

```cpp
class LegacyGlobalFreightRateApi {
public:
    explicit LegacyGlobalFreightRateApi(long surcharge_cents);

    // zone_code: 0 = local, 1 = regional, 2 = national, 3 = international.
    long lookup_rate_cents_for_parcel(long weight_grams, int zone_code) const;

private:
    long surcharge_cents_;
};
```

Every one of this chapter's four techniques touches this one integration from a different angle: Adapter makes `LegacyGlobalFreightRateApi` usable where this book's code expects a `RateSource`, Facade hides that a quote, a label, and a tracking number are three separate calls at all, Bridge decouples the shipment-status messages this integration triggers from how those messages get delivered, and PImpl keeps the label's exact formatting out of a header anything else in the codebase has to recompile against.

## Adapter: wrapping a legacy rate API

`RateSource` is the target interface: the one shape checkout code, and the Facade built later in this chapter, actually depend on. It says nothing about GlobalFreight, grams, or zone codes.

```cpp
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};
```

`LegacyFreightRateAdapter` is the Adapter. It implements `RateSource`, and it does the translating: kilograms to grams, `DestinationZone` to a zone code, on every call.

```cpp
class LegacyFreightRateAdapter : public RateSource {
public:
    explicit LegacyFreightRateAdapter(legacy_freight::LegacyGlobalFreightRateApi legacy_api);
    Cents quote(const Shipment& shipment) const override;

private:
    legacy_freight::LegacyGlobalFreightRateApi legacy_api_;
};

Cents LegacyFreightRateAdapter::quote(const Shipment& shipment) const {
    const long weight_grams = static_cast<long>(shipment.weight_kg * 1000.0);
    const int zone_code = zone_to_legacy_code(shipment.zone);
    return legacy_api_.lookup_rate_cents_for_parcel(weight_grams, zone_code);
}
```

The member `legacy_api_` is what makes this an object adapter rather than a class adapter: `LegacyFreightRateAdapter` *holds* a `LegacyGlobalFreightRateApi` by composition, instead of inheriting from it. That distinction matters for a reason beyond style. C++ allows a class adapter, built with private inheritance from the legacy type, and it would compile and pass the same tests. But private inheritance ties this adapter to one specific legacy class for its whole lifetime, exposes that relationship to anything inspecting the adapter's base classes, and breaks down the moment two legacy APIs need wrapping behind the same `RateSource` by one adapter, since C++ has no trouble with multiple inheritance but a reader absolutely does. Composition keeps `LegacyFreightRateAdapter` a small, ordinary class holding a value it happens to translate for, free to wrap a different legacy type, or two of them, without touching its own inheritance hierarchy at all. This book defaults to the object adapter everywhere a legacy wrapper comes up, and the choice stands unless something specific about a legacy type forces otherwise, which does not happen in this example.

Nothing about `RateSource` or any of its callers mentions GlobalFreight. Swap `LegacyFreightRateAdapter` for a different carrier's adapter and every caller of `RateSource::quote()` keeps working unmodified, which is the whole point of adapting to a target interface instead of threading the legacy shape through the rest of the codebase.

## Facade: one call over three subsystems

A shipment needs three things that have to happen in the right order and land in a consistent result: a rate quote, a label with the right tracking number already printed on it, and a tracking URL a customer can visit. `TrackingRegistry` issues the tracking number and builds that URL.

```cpp
class TrackingRegistry {
public:
    explicit TrackingRegistry(std::string carrier_domain);
    std::string issue_tracking_number(int sequence) const;
    std::string tracking_url(const std::string& tracking_number) const;

private:
    std::string carrier_domain_;
};
```

`ShippingFacade` is the Facade. It owns a `RateSource`, a label renderer, and a `TrackingRegistry`, and it exposes exactly one method that matters to a caller: `quote_and_label()`.

```cpp
class ShippingFacade {
public:
    ShippingFacade(std::unique_ptr<RateSource> rate_source, LabelRenderer label_renderer,
                   TrackingRegistry tracking_registry);
    ShippingQuote quote_and_label(const Shipment& shipment, int sequence) const;

private:
    std::unique_ptr<RateSource> rate_source_;
    LabelRenderer label_renderer_;
    TrackingRegistry tracking_registry_;
};

ShippingQuote ShippingFacade::quote_and_label(const Shipment& shipment, int sequence) const {
    if (shipment.weight_kg < 0.0) {
        throw std::invalid_argument("shipment weight_kg cannot be negative");
    }
    const Cents price = rate_source_->quote(shipment);
    const std::string tracking_number = tracking_registry_.issue_tracking_number(sequence);
    const std::string label = label_renderer_.render(shipment, tracking_number);
    const std::string url = tracking_registry_.tracking_url(tracking_number);
    return ShippingQuote{price, label, tracking_number, url};
}
```

The sequencing inside `quote_and_label()` is the actual value this Facade delivers, not the three subsystem calls on their own. The tracking number has to exist before the label can print it, and the label has to be built from the same shipment the quote priced. A checkout handler calling `rate_source_->quote()`, `tracking_registry_.issue_tracking_number()`, and `label_renderer_.render()` directly would have to get that order right every single time it wrote this logic, in every place it wrote this logic. `ShippingFacade` gets it right once, in one method, and every caller inherits that correctness instead of re-deriving it. That is what a Facade buys beyond convenience: it is not shorter code, it is one place the sequencing invariant lives instead of being trusted to every caller's memory.

Nothing about this Facade adds a new capability `RateSource`, `LabelRenderer`, and `TrackingRegistry` did not already have separately. A reviewer could, in principle, call the three subsystems directly and get the same result for one shipment. The Facade's value shows up over time and across callers: a second checkout path, or a batch job quoting a thousand shipments overnight, inherits the correct sequencing instead of re-implementing it with its own chance to get it wrong.

## Bridge: notifications that vary two ways

A shipment triggers notifications, and two things about a notification vary independently of each other. The kind of event varies: a shipment was created, or a delivery is delayed, each needing its own message text. The delivery channel also varies: email, SMS, or whatever a future integration adds, each needing its own way of actually sending something. `NotificationTransport` is the implementation side of the Bridge, the delivery mechanism.

```cpp
class NotificationTransport {
public:
    virtual ~NotificationTransport() = default;
    virtual std::string deliver(const std::string& recipient, const std::string& message) const = 0;
};

class EmailTransport : public NotificationTransport {
public:
    std::string deliver(const std::string& recipient, const std::string& message) const override;
};

class SmsTransport : public NotificationTransport {
public:
    std::string deliver(const std::string& recipient, const std::string& message) const override;
};
```

`Notification` is the abstraction side. It holds a `NotificationTransport` by pointer, not by inheriting from it, and its `send()` method is the bridge between the two axes: it asks the concrete notification for its message text, then hands that text to whichever transport was configured.

```cpp
class Notification {
public:
    explicit Notification(std::shared_ptr<NotificationTransport> transport);
    virtual ~Notification() = default;
    std::string send(const std::string& recipient) const;

protected:
    virtual std::string format_message() const = 0;

private:
    std::shared_ptr<NotificationTransport> transport_;
};

std::string Notification::send(const std::string& recipient) const {
    return transport_->deliver(recipient, format_message());
}
```

`ShipmentCreatedNotification` and `DeliveryDelayedNotification` are the refined abstractions. Each overrides `format_message()` with its own text and leaves `send()`, and the transport it was built with, untouched:

```cpp
class ShipmentCreatedNotification : public Notification {
public:
    ShipmentCreatedNotification(std::shared_ptr<NotificationTransport> transport, std::string tracking_number);

protected:
    std::string format_message() const override;

private:
    std::string tracking_number_;
};
```

This is the structural reason Bridge is a separate pattern from plain polymorphism. Two refined `Notification` kinds and two `NotificationTransport` implementations produce four working combinations, a shipment-created email, a shipment-created text, a delay email, a delay text, and none of those four combinations needed its own class. Without the Bridge, the next instinct is usually a class per combination: `ShipmentCreatedEmailNotification`, `ShipmentCreatedSmsNotification`, `DeliveryDelayedEmailNotification`, `DeliveryDelayedSmsNotification`, and the count keeps multiplying every time either axis grows by one. A third notification kind or a third transport added to the Bridge's two existing hierarchies is one new class, not a new row and column of classes crossed against everything that already exists.

## PImpl: a compilation firewall that still behaves like a value

`LabelRenderer` produces the label text `ShippingFacade` prints onto a `ShippingQuote`, and exactly how that text is laid out, what order the fields print in, what separators sit between them, is a decision nothing outside this one class needs to see. Its header declares the operations and nothing about the layout:

```cpp
class LabelRenderer {
public:
    explicit LabelRenderer(std::string carrier_name);
    ~LabelRenderer();

    LabelRenderer(const LabelRenderer& other);
    LabelRenderer& operator=(const LabelRenderer& other);
    LabelRenderer(LabelRenderer&& other) noexcept;
    LabelRenderer& operator=(LabelRenderer&& other) noexcept;

    std::string render(const Shipment& shipment, const std::string& tracking_number) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

`Impl` is declared here but never defined; its definition, and every formatting detail, lives entirely in `label_renderer.cpp`:

```cpp
class LabelRenderer::Impl {
public:
    explicit Impl(std::string carrier_name) : carrier_name_(std::move(carrier_name)) {}

    std::string render(const Shipment& shipment, const std::string& tracking_number) const {
        std::ostringstream label;
        label << carrier_name_ << " LABEL | weight=" << shipment.weight_kg << "kg"
              << " | zone=" << zone_name(shipment.zone) << " | tracking=" << tracking_number;
        return label.str();
    }

private:
    std::string carrier_name_;
};
```

Changing that layout, adding a field, reordering the separators, never touches `label_renderer.hpp`, which means nothing that only calls `render()` ever needs to recompile when the layout changes. That is the compilation firewall PImpl is named for, and it is a real, measurable property on a large codebase, not an abstract benefit: a header that changes forces every translation unit including it to recompile, and a header with no implementation detail in it changes far less often than one with the formatting logic inlined.

The cost of that firewall is that `LabelRenderer`'s special member functions no longer come from the compiler automatically. A class whose only data member is a `std::unique_ptr<Impl>` to an incomplete type needs an out-of-line destructor, because `std::default_delete<Impl>` has to see `Impl`'s full definition to destroy it, and that definition only exists in the `.cpp` file. The same incompleteness means the compiler-generated copy constructor, which would copy the `unique_ptr` itself, has to be replaced by hand with one that copies what the pointer points to:

```cpp
LabelRenderer::~LabelRenderer() = default;

LabelRenderer::LabelRenderer(const LabelRenderer& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

LabelRenderer& LabelRenderer::operator=(const LabelRenderer& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}
```

Move construction and move assignment need no such care. Moving a `unique_ptr` only transfers ownership of the pointer, which never requires seeing what it points to, so `= default` is correct and sufficient for both:

```cpp
LabelRenderer::LabelRenderer(LabelRenderer&& other) noexcept = default;
LabelRenderer& LabelRenderer::operator=(LabelRenderer&& other) noexcept = default;
```

The test worth running against any PImpl type is whether it still behaves like an ordinary value after all of this. Copying a `LabelRenderer` and rendering the same shipment through the original and the copy has to produce identical text, proving the copy did not just duplicate a pointer into shared state. And `LabelRenderer`'s own size tells the rest of the story: with exactly one data member, a `std::unique_ptr<Impl>`, `sizeof(LabelRenderer)` equals one pointer, regardless of how much formatting logic `Impl` eventually grows to hold.

> **AI sidebar.** Ask an LLM pairing partner to "wrap this third-party rate API" and a frequent result reaches straight for inheritance: `class LegacyFreightRateAdapter : public RateSource, private LegacyGlobalFreightRateApi`. It compiles, and for a single legacy type it even works. Check what it costs before accepting it: private inheritance exposes the legacy relationship in the adapter's own class declaration, and it has no clean answer the day a second legacy API needs wrapping behind the same interface by the same adapter. The object-adapter shape this chapter builds, composing the legacy type as a plain data member, scales to that case without changing shape at all. A second thing worth checking in generated Facade code: does the generated method still let a caller reach the subsystem objects directly, through a public accessor or a returned reference? A `ShippingFacade` with a `rate_source()` getter next to `quote_and_label()` is a Facade that forgot its own job, leaving the sequencing invariant optional for any caller willing to skip the one method that enforces it.

## A typed rejection: C++20 versus C++23

`quote_and_label()` throws `std::invalid_argument` on a shipment with a negative weight, which works but forces every caller either to wrap the call in a `try` block or accept that an unhandled exception can propagate out of checkout. C++23's `std::expected` gives the same rejection a typed alternative: a return type carrying either the completed quote or a specific reason it was rejected, checkable with an `if` and no exception machinery at all. `try_quote_and_label()` in this example is guarded behind `#if __cplusplus >= 202302L` precisely so the mandatory C++20 build path never needs `std::expected`, while the C++23 build gets the more precise contract.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on a malformed shipment works, but it forces every
// caller to wrap the call in a try block or accept an unhandled exception
// on a checkout path.
ShippingQuote ShippingFacade::quote_and_label(const Shipment& shipment, int sequence) const {
    if (shipment.weight_kg < 0.0) {
        throw std::invalid_argument("shipment weight_kg cannot be negative");
    }
    const Cents price = rate_source_->quote(shipment);
    const std::string tracking_number = tracking_registry_.issue_tracking_number(sequence);
    const std::string label = label_renderer_.render(shipment, tracking_number);
    const std::string url = tracking_registry_.tracking_url(tracking_number);
    return ShippingQuote{price, label, tracking_number, url};
}
```
```cpp
// C++23: std::expected reports the same rejection as a typed error,
// checkable with an if and no exception machinery.
std::expected<ShippingQuote, std::string> ShippingFacade::try_quote_and_label(const Shipment& shipment,
                                                                               int sequence) const {
    if (shipment.weight_kg < 0.0) {
        return std::unexpected<std::string>("shipment weight_kg cannot be negative");
    }
    const Cents price = rate_source_->quote(shipment);
    const std::string tracking_number = tracking_registry_.issue_tracking_number(sequence);
    const std::string label = label_renderer_.render(shipment, tracking_number);
    const std::string url = tracking_registry_.tracking_url(tracking_number);
    return ShippingQuote{price, label, tracking_number, url};
}
```

Both versions run the identical three-subsystem sequence for a valid shipment. Only how a caller learns about a rejected one changes between the two columns, the same narrow scope Chapter 8's and Chapter 12's codetabs held to for their own typed-error refinements.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/15-adapter-facade-bridge` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 17 cases under the `cpp23` preset, 15 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8, building this example standalone from its own directory:

```
100% tests passed, 0 tests failed out of 17   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

To see all four techniques produce real output rather than just reading an assertion that says they work, run the example's own demo script:

```sh
examples/15-adapter-facade-bridge/demo.sh cpp23
```

It adapts a legacy quote, runs the Facade over one shipment, copies a PImpl `LabelRenderer`, and sends two notification kinds over two transports:

```
== Adapter: the legacy GlobalFreight API behind RateSource ==
Adapted quote: 600 cents

== Facade: one call fronts rating, labeling, and tracking ==
price:    600 cents
label:    GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-1001
tracking: https://track.globalfreight.example/track/TRK-1001

== PImpl: LabelRenderer copies correctly, header hides its format ==
original: GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-9001
copy:     GlobalFreight LABEL | weight=3.5kg | zone=Regional | tracking=TRK-9001

== Bridge: two notification kinds, two transports, no class explosion ==
EMAIL to customer@example.com: Your shipment TRK-1001 has been created.
SMS to +1-555-0100: Your shipment TRK-1001 has been created.
EMAIL to customer@example.com: Shipment TRK-1001 is delayed by 6 hours.
SMS to +1-555-0100: Shipment TRK-1001 is delayed by 6 hours.

== C++23 refinement: typed rejection via std::expected ==
Rejected malformed shipment: shipment weight_kg cannot be negative
```

Notice that the original `LabelRenderer` and its copy print byte-identical labels for the same shipment and tracking number. That agreement is the PImpl copy constructor's deep copy showing up as an observable fact at the terminal, not just as a passing test assertion. Running `examples/15-adapter-facade-bridge/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## When not to use Adapter, Facade, Bridge, or PImpl

All four techniques in this chapter earn their place when a specific, named friction exists: an interface mismatch to bridge, an orchestration sequence to protect, two independent axes of variation to decouple, or a header that changes too often. Used without one of those frictions present, each adds a layer with nothing underneath it.

Adapter needs an actual mismatch to resolve. If `LegacyGlobalFreightRateApi` already took a `Shipment` and returned a `Cents`, wrapping it in `LegacyFreightRateAdapter` would add a class and a virtual call around a method that could be called directly. This adapter is justified because the legacy API's units and names differ from what the rest of the codebase expects; writing one preemptively, in case some future API someday needs wrapping, defends against a mismatch that does not exist yet.

Facade needs more than one subsystem with a real sequencing or coordination rule between them. A single subsystem, called through a method that does nothing but forward to it, is not a Facade; it is an unnecessary layer of indirection with a different name. The signal that `ShippingFacade` is pulling its weight is the `if` check and the three-call sequence inside `quote_and_label()`, not its three constructor parameters on their own. A class with three constructor parameters and no sequencing logic between the calls it makes is closer to a parameter struct than to this pattern.

Bridge needs two axes that vary independently in practice, not one axis dressed up as two. If this example only ever sent `ShipmentCreatedNotification` over email, with no delay notification and no second transport planned, splitting `Notification` from `NotificationTransport` buys nothing: there would be exactly one combination, achievable with one class and no bridge at all. The moment to introduce the split is the moment a second kind or a second transport is either in hand or clearly coming, the same test Chapter 12 applied to Abstract Factory's family of products.

PImpl costs real code, the hand-written destructor, copy constructor, and copy assignment operator this chapter walked through, in exchange for a compilation firewall. That trade only pays off when the class's implementation detail is either large, likely to change, or exposes a dependency, a third-party header, say, that the rest of the codebase should not have to include transitively. A small, stable class with no such dependency gains nothing from PImpl beyond the maintenance burden of four extra hand-written member functions. `LabelRenderer` earns the idiom here because its formatting logic is exactly the kind of detail that changes as label requirements evolve, and because keeping that churn out of the header keeps every file that only calls `render()` from recompiling every time it does.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley): the original Adapter, Facade, and Bridge entries, including the object-adapter-versus-class-adapter distinction this chapter's `LegacyFreightRateAdapter` follows and the compilation-firewall motivation this chapter's PImpl section builds on.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/15-adapter-facade-bridge`'s suite, which asserts that the adapted quote matches the legacy math, that the facade's sequencing produces a consistent result, that two notification kinds compose with two transports without a class per combination, and that the PImpl type copies and moves correctly rather than only checking that each binary exits zero.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
