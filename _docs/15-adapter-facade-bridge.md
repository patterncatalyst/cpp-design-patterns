---
title: "Adapter, Facade, Bridge, and PImpl"
order: 15
part: "composing-adapting-structure"
description: "Four structural techniques taught in one cohesive shipping subsystem: an Adapter that wraps a legacy carrier rate API behind a domain interface, a Facade that fronts rating, labeling, and tracking with one call, a Bridge that lets a notification's kind and its delivery transport vary independently, and the PImpl idiom hiding a label renderer's formatting behind a compilation firewall."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force each of Adapter, Facade, and Bridge resolves, and tell the three apart from each other and from PImpl.
- Wrap a legacy, differently shaped rate API behind your own `RateSource` interface by composition.
- Give a client one method over several subsystems, so the call order lives in one place.
- Let a `Notification`'s kind and its delivery transport vary independently, without a class per combination.
- Give a class a PImpl compilation firewall behind `std::unique_ptr<Impl>` and keep it copyable and movable.
- Choose between a throwing lookup and `std::expected`, new in C++23, for a facade's rejected-input path.
- Recognize when a plain function call or a visible member already solves the problem.

## Why this matters

A checkout flow needs a shipping quote, a label and a tracking number. The rate comes from a carrier API older than the codebase, with its own units and method names. The quote, the label and the tracking number are three calls that must happen together and in order, or a label ships without a matching rate. When a shipment's status changes, someone must hear about it by their preferred channel, in a message that depends on what happened.

These are structural problems: giving an awkward class a usable shape, giving a client one thing to call, and decoupling two things that vary on their own. Adapter, Facade and Bridge are the names for those three. PImpl answers a narrower question: how to keep a class's header stable while its implementation changes.

{% include excalidraw.html name="15-adapter-facade-bridge" caption="Figure 15.1: Adapter wraps a legacy API behind a target interface; Facade fronts three subsystems with one call; Bridge crosses an abstraction axis of refined notification kinds with an implementation axis of transports." %}

All four work on one carrier integration, GlobalFreight. `Shipment` carries a weight, a destination zone and a declared value:

```cpp
enum class DestinationZone { Local, Regional, National, International };
using Cents = std::int64_t;

struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};
```

## Pattern: Adapter

### The problem

`LegacyGlobalFreightRateApi` is a third-party rate lookup you can't redesign. Weight travels in grams, the destination as a numeric zone code, and the quote comes back as a plain `long`:

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

Threading grams and zone codes through checkout code ties every caller to one carrier.

### The solution

**An Adapter converts the interface of an existing class into the interface its clients expect.**

### How C++ models it

`RateSource` is the target interface. It says nothing about GlobalFreight, grams or zone codes:

```cpp
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};
```

`LegacyFreightRateAdapter` implements it and translates on every call:

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

A 3.5 kg regional shipment becomes 3,500 grams and zone code 1, and the legacy table prices it at 600 cents. Swap in another carrier's adapter and every caller of `RateSource::quote()` keeps working unchanged.

### Object adapter or class adapter

The member `legacy_api_` makes this an object adapter: it holds the legacy type instead of inheriting from it.

| | Object adapter (composition) | Class adapter (private inheritance) |
| --- | --- | --- |
| Legacy type | A data member | A base class |
| Visible in the class declaration | No | Yes, in the base list |
| Wrapping a second legacy API | Add a member | A second base, harder to read |
| Swapping the wrapped type | Change one member | Change the hierarchy |

Both compile and pass the same tests. Default to the object adapter unless the legacy type forces otherwise, for example by exposing a protected member you must override.

## Pattern: Facade

### The problem

A shipment needs a rate quote, a label with the tracking number printed on it, and a tracking URL. The tracking number must exist before the label can print it, and the label must be built from the shipment the quote priced. Every caller that makes the three calls itself must get that order right, in every place it does so.

### The solution

**A Facade gives one simple interface to a set of subsystems and holds the coordination between them.**

### How C++ models it

`TrackingRegistry` issues tracking numbers and builds URLs:

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

`ShippingFacade` owns a `RateSource`, a `LabelRenderer` and a `TrackingRegistry`, and exposes one method that matters to a caller:

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
```

The Facade adds no capability the three subsystems lack. What it adds is one place where the sequencing rule lives. A second checkout path, or a batch job quoting a thousand shipments overnight, inherits the correct order instead of re-implementing it. A Facade that also exposes a `rate_source()` getter makes that rule optional again.

### A typed rejection

`quote_and_label()` throws `std::invalid_argument` on a negative weight, so every caller wraps the call in `try` or lets the exception leave checkout. `std::expected`, new in C++23, returns either the quote or the reason it was rejected, checkable with an `if`. `try_quote_and_label()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never sees it.

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

A valid shipment runs the same three-subsystem sequence in both. Only how a caller learns about a rejection changes.

## Pattern: Bridge

### The problem

Two things about a notification vary independently. The event varies: a shipment was created, or a delivery is delayed, each with its own text. The channel varies: email, SMS, or whatever comes next. A class per combination (`ShipmentCreatedEmailNotification`, `ShipmentCreatedSmsNotification`, and so on) multiplies every time either axis grows.

### The solution

**A Bridge splits an abstraction from its implementation into two hierarchies joined by a reference, so each can vary on its own.**

{% include excalidraw.html name="15-bridge-matrix" caption="Figure 15.2: Two notification kinds crossed with two transports give four live combinations from N + M classes, not the N x M classes a class-per-combination design would need." %}

### How C++ models it

`NotificationTransport` is the implementation side:

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

`Notification` is the abstraction side. It holds a transport by pointer, and `send()` is the bridge: it asks the concrete notification for its text and hands it to the transport.

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

Each refined abstraction overrides only `format_message()`:

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

`ShipmentCreatedNotification` and `DeliveryDelayedNotification` crossed with `EmailTransport` and `SmsTransport` give four working combinations from four classes. A third kind or a third transport is one new class.

## Pattern: PImpl

### The problem

`LabelRenderer` lays out the label text: field order, separators, units. Nothing outside the class needs that detail, but if it sits in the header, every translation unit that includes the header recompiles each time the layout changes.

### The solution

**PImpl (pointer to implementation) moves a class's private state and logic into a type defined only in the source file, reached through one pointer.**

### How C++ models it

The header declares the operations and an incomplete `Impl`:

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

The layout lives in `label_renderer.cpp`:

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

Adding a field or reordering separators never touches `label_renderer.hpp`, so code that only calls `render()` doesn't recompile. `sizeof(LabelRenderer)` is one pointer however large `Impl` grows.

### The special members

The firewall costs the compiler-generated special members. `std::default_delete<Impl>` must see the full definition of `Impl`, so the destructor is defined out of line where `Impl` is complete. The generated copy would copy the `unique_ptr`, which isn't copyable, so copy construction and assignment copy the pointee by hand:

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

Moving a `unique_ptr` only transfers the pointer, so both moves are `= default`, defaulted in the source file beside the destructor:

```cpp
LabelRenderer::LabelRenderer(LabelRenderer&& other) noexcept = default;
LabelRenderer& LabelRenderer::operator=(LabelRenderer&& other) noexcept = default;
```

A copy renders byte-identical text to its original, so the type still behaves like a value.

## When not to use it

Each technique needs a named friction. Without one, it adds a layer with nothing underneath.

- **Adapter.** Use it for an actual mismatch. If the legacy API already took a `Shipment` and returned `Cents`, the adapter would add a class and a virtual call around a method you could call directly. Don't write one in case a future API needs wrapping.
- **Facade.** Use it for several subsystems with a sequencing or coordination rule between them. A method that only forwards to one subsystem is indirection with a different name. The validation and the three-call order inside `quote_and_label()` justify `ShippingFacade`; its three constructor parameters don't.
- **Bridge.** Use it for two axes that vary in practice. One notification kind over one transport is one class. Split when a second kind or transport is in hand or clearly coming.
- **PImpl.** It costs a hand-written destructor, copy constructor and copy assignment, plus a heap allocation and an indirection per object. It pays when the implementation is large, changes often, or pulls in a third-party header the rest of the code shouldn't include. A small, stable class gains nothing.

> **🤖 Explore with your agent**
>
> "Write `LegacyFreightRateAdapter` implementing `RateSource`, holding `LegacyGlobalFreightRateApi` as a private data member rather than inheriting from it, converting kilograms to grams and `DestinationZone` to the legacy zone code. Write `ShippingFacade` with one public `quote_and_label()` that quotes, issues the tracking number, then renders the label, and no accessors for its subsystems."
>
> Run `ctest --preset cpp23 -R 'LegacyFreightRateAdapter|ShippingFacade'`, and check that the adapter's base list names only `RateSource` and that `ShippingFacade` has no subsystem getter.

## Build, run, observe

From the repository root, the superbuild picks up `examples/15-adapter-facade-bridge`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 17 cases pass under `cpp23` and 15 under `cpp20`; the two `std::expected` cases compile only under C++23. To see the four techniques run:

```sh
examples/15-adapter-facade-bridge/demo.sh cpp23
```

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

The original renderer and its copy print the same label. Under `cpp20` the output stops before the `std::expected` block.

- **Default the destructor in the header.** Change `~LabelRenderer();` to `~LabelRenderer() = default;` and delete the out-of-line definition. The build fails with `invalid application of 'sizeof' to an incomplete type 'shipping::LabelRenderer::Impl'`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Adapter, Facade, and Bridge entries, including the object-adapter versus class-adapter distinction and the compilation-firewall motivation behind PImpl.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/15-adapter-facade-bridge`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
