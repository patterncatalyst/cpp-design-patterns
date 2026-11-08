---
title: "Large-scale C++: physical design, reviewed in code"
order: 39
layout: tutorial
sectionid: docs
part: appendices
appendix: "G"
description: "A code-first review of John Lakos's physical-design discipline for C++, the component, levelization, and insulation, on the shipping domain."
---

## Learning objectives

By the end of this appendix you can:

- Split a god header into well-formed components.
- Break an include cycle by escalation, demotion, an opaque pointer, a manager class or dumb data.
- Tell insulation from encapsulation and pick PImpl, a protocol class or a procedural interface.
- Cut compile-time coupling with forward declarations and include-what-you-use.

## Why this matters

Logical design decides which class does what. Physical design decides which file includes which, and what that costs at compile time, at link time and in what can be tested alone. Four facts make it a C++ concern:

| Fact | Consequence |
| --- | --- |
| Separate compilation | A `.cpp` is translated alone; every outside fact arrives through a header it includes. |
| Textual inclusion | `#include` copies text before the compiler sees a type, so a header's cost is paid in every file that includes it. |
| One-definition rule | Every translation unit that sees a type's definition must see the identical one; a header seen differently is an ODR violation the compiler needn't diagnose. |
| Separate linking | Symbols resolve after every translation unit compiles, so no single compile shows the component graph. A dependency cycle surfaces as components that can't be built or tested apart. |

Java, Python and C# load metadata or cached modules, not textual headers, so they don't have this combination of forces, and the vocabulary here (component, levelization, insulation) is C++-specific. The source is John Lakos's *Large-Scale C++ Volume I: Process and Architecture*. A design can be logically clean, every class well-named and singly responsible, and still be unbuildable in isolation and expensive to change because one header touches half the codebase.

## The component

A component is one header and its matching source file, treated as one unit: `rate_quote.h` and `rate_quote.cpp`. It's the unit of testing, reuse and building. A class is a logical idea; the component is where it lives on disk, one class per component plus the helpers that class can't work without.

| Rule | Example | What it catches |
| --- | --- | --- |
| Logical and physical names match | `carrier_gateway.h` declares `CarrierGateway` | `shipping_utils.h` declaring `Invoice` |
| The `.cpp` includes its own header first | `carrier_gateway.cpp` starts with `#include "carrier_gateway.h"` | A header that compiles only because `<string>` happened to come earlier |
| One primary class | Helpers allowed: an iterator, a small value type a method returns | Two independently useful classes that can't be tested, reused or linked apart |
| One test driver | Lakos names it `carrier_gateway.t.cpp`; the examples use one GoogleTest target per component | Changes to `CarrierGateway` with no single place to add a case |

A god header breaks all four:

```cpp
// Violates component discipline: shipping_types.h is a god header
// declaring three unrelated, independently useful classes. No one
// can include RateQuote without also compiling Invoice and Manifest
// into every translation unit that needs it, and no one can write a
// test driver for just one of the three.
#ifndef SHIPPING_TYPES_H
#define SHIPPING_TYPES_H

#include <string>
#include <vector>

class RateQuote {
public:
    RateQuote(std::string carrier_code, long cents);
    long amount_cents() const { return cents_; }
private:
    std::string carrier_code_;
    long cents_;
};

class Invoice {
public:
    void add_line_item(std::string description, long cents);
    long total_cents() const;
private:
    std::vector<std::pair<std::string, long>> line_items_;
};

class Manifest {
public:
    void add_shipment(std::string tracking_id);
    std::size_t shipment_count() const { return tracking_ids_.size(); }
private:
    std::vector<std::string> tracking_ids_;
};

#endif
```

Split into components:

```cpp
// Honors component discipline: three components, each with its own
// header, source file, and test driver. rate_quote.cpp includes
// "rate_quote.h" first, proving the header is self-sufficient.

// rate_quote.h
#ifndef RATE_QUOTE_H
#define RATE_QUOTE_H

#include <string>

class RateQuote {
public:
    RateQuote(std::string carrier_code, long cents);
    long amount_cents() const { return cents_; }
private:
    std::string carrier_code_;
    long cents_;
};

#endif

// rate_quote.cpp
#include "rate_quote.h"  // first, substantive, proves self-sufficiency

RateQuote::RateQuote(std::string carrier_code, long cents)
    : carrier_code_(std::move(carrier_code)), cents_(cents) {}
```

`invoice.h`/`invoice.cpp` and `manifest.h`/`manifest.cpp` take the same shape, each with its own driver. A client that needs only `RateQuote` compiles and links only `rate_quote.cpp`.

## Physical dependencies and levelization

Component A physically depends on B when A can't compile, link or run its tests without B. That's stricter and more measurable than "A uses B": it shows up as an `#include`, a symbol resolved at link time, or a test that must build a real B because no seam exists.

Draw components as nodes and physical dependencies as edges, and you have the include graph. Lakos's central rule: it must be a directed acyclic graph. If A depends on B and B on A, neither can be compiled, linked or tested without the other, and there's no smaller unit left to test in isolation.

In an acyclic graph, a component that depends on nothing in its group is level 1, and a component whose highest dependency is level N-1 is level N. Levels give a test order: level 1 first, with nothing below to fake; level 2 next, against level-1 components already known good; and so on. A script can compute the order from the include graph.

`Shipment` and `Carrier` invite a cycle: a shipment validates its destination against a carrier's service area, and a carrier quotes from a shipment's weight.

### Escalation

Move the operation that needs both types into a higher-level component that depends on both peers.

```cpp
// Before: Shipment depends on Carrier to validate itself, and
// Carrier depends on Shipment to quote a rate. Cycle.
// shipment.h
#include "carrier.h"
class Shipment {
public:
    bool is_valid_for(const Carrier& carrier) const;
    double weight_kg() const { return weight_kg_; }
private:
    double weight_kg_;
};

// carrier.h
#include "shipment.h"
class Carrier {
public:
    long quote_rate_cents(const Shipment& shipment) const;
};
```

```cpp
// After: escalation. Shipment and Carrier no longer include each
// other; shipment_carrier_rules.h sits above both and owns the
// operation that needed to see both types at once.
// shipment.h: no dependency on Carrier
class Shipment {
public:
    double weight_kg() const { return weight_kg_; }
    std::string destination_zone() const { return destination_zone_; }
private:
    double weight_kg_;
    std::string destination_zone_;
};

// carrier.h: no dependency on Shipment
class Carrier {
public:
    bool services_zone(std::string_view zone) const;
    long base_rate_cents(double weight_kg) const;
};

// shipment_carrier_rules.h: level 2, depends on both level-1 peers
#include "carrier.h"
#include "shipment.h"

inline bool is_valid_for(const Shipment& shipment, const Carrier& carrier) {
    return carrier.services_zone(shipment.destination_zone());
}

inline long quote_rate_cents(const Carrier& carrier, const Shipment& shipment) {
    return carrier.base_rate_cents(shipment.weight_kg());
}
```

### Demotion

When the mutual dependency is one shared fact, move that fact into a lower-level component both peers include.

```cpp
// Before: Carrier needs Shipment's packaging category, and Shipment
// needs Carrier's surcharge table, both keyed on the same code.
// A shared ShippingClass enum lives inline in each header,
// duplicated and prone to drifting out of sync.

// After: demotion. shipping_class.h is a new level-1 component
// holding only the shared vocabulary; both Shipment and Carrier
// depend on it, and neither depends on the other.
// shipping_class.h
enum class ShippingClass { Standard, Fragile, Hazardous };

// shipment.h
#include "shipping_class.h"
class Shipment {
public:
    ShippingClass shipping_class() const { return shipping_class_; }
private:
    ShippingClass shipping_class_;
};

// carrier.h
#include "shipping_class.h"
class Carrier {
public:
    long surcharge_cents(ShippingClass shipping_class) const;
};
```

### Opaque pointer

When one peer only holds a pointer or reference to the other, forward-declare it and move the include into the `.cpp` that dereferences it.

```cpp
// Before: shipment.h includes carrier.h to hold a pointer, which
// creates a compile-time dependency Shipment does not actually need.
// shipment.h
#include "carrier.h"
class Shipment {
public:
    void assign_carrier(Carrier* carrier) { carrier_ = carrier; }
private:
    Carrier* carrier_ = nullptr;
};
```

```cpp
// After: opaque pointer. shipment.h forward-declares Carrier; the
// definition is only needed in shipment.cpp, wherever carrier_ is
// actually dereferenced.
// shipment.h
class Carrier;  // forward declaration, no physical dependency here

class Shipment {
public:
    void assign_carrier(Carrier* carrier) { carrier_ = carrier; }
    long quoted_rate_cents() const;  // defined in shipment.cpp
private:
    Carrier* carrier_ = nullptr;
};

// shipment.cpp
#include "shipment.h"
#include "carrier.h"  // the real dependency moves here

long Shipment::quoted_rate_cents() const {
    return carrier_->base_rate_cents(weight_kg());
}
```

### Manager class and dumb data

{% include excalidraw.html name="39-levelization-before-after" caption="Figure G.1: Shipment and Carrier as a cyclic include graph, redrawn as a levelized DAG after escalating the shared operation into a manager-class coordinator." %}

A manager class is escalation into a stateful component. In Figure G.1, `ShipmentCarrierCoordinator` at level 2 owns the operation that needs both peers, and `Shipment` and `Carrier` at level 1 never name each other. Prefer it over a free function once the coordinating logic holds its own data, such as a cache of recent quotes.

Dumb data: one component carries a value another interprets. A `TrackingRecord` stores a carrier-specific status code as a plain integer and never needs to know `Carrier` exists; only the component that interprets the code depends on `Carrier`.

| Technique | Use when | Adds |
| --- | --- | --- |
| Escalation | An operation needs both peers | A higher-level component |
| Demotion | The peers share one fact | A lower-level component |
| Opaque pointer | One peer only points at the other | A forward declaration; the include moves to the `.cpp` |
| Manager class | The coordinating logic holds state | A stateful higher-level component |
| Dumb data | One side stores a value the other interprets | Untyped data at the boundary |

Every technique adds a component, a declaration or an indirection. A codebase with one real two-way relationship and no plan to grow can merge the two types into one component and document it. Levelization pays off once a third component needs one peer without the other, which a cyclic pair can't support.

## Insulation versus encapsulation

Encapsulation hides a detail from logical access: a client can't read a private member, so its correctness can't depend on it. But the member still sits in the header. Add, remove or retype it, and every translation unit that includes the header recompiles. The member is encapsulated, not insulated.

Insulation means a change to the detail doesn't recompile clients at all, because the detail lives in another file behind a layer that doesn't change. The price is paid on every use: an indirection, a heap allocation for the hidden object, or a virtual call.

### PImpl

The stable header holds a pointer to a forward-declared implementation class, whose definition and private members live in the `.cpp`. [Type erasure and PImpl]({{ '/docs/22-type-erasure-and-pimpl/' | relative_url }}) builds it in full; for Lakos it's one insulation technique among several, the one that keeps the type's name and public signatures in the header.

```cpp
// carrier_gateway.h: stable, included by every client. Changing
// CarrierGatewayImpl's data members never touches this file.
#include <memory>

class CarrierGateway {
public:
    CarrierGateway();
    ~CarrierGateway();
    long request_rate_quote_cents(double weight_kg) const;
private:
    class CarrierGatewayImpl;
    std::unique_ptr<CarrierGatewayImpl> impl_;
};
```

### Protocol class

A pure abstract base with only public virtual methods and a virtual destructor, no data members. The concrete class lives in the `.cpp`, reachable through a factory the header declares.

```cpp
// carrier_gateway.h: a protocol class. No concrete type, no data
// member, nothing for a client to depend on but the interface.
#include <memory>

class CarrierGateway {
public:
    virtual ~CarrierGateway() = default;
    virtual long request_rate_quote_cents(double weight_kg) const = 0;
};

std::unique_ptr<CarrierGateway> make_carrier_gateway(std::string_view carrier_code);

// carrier_gateway.cpp: the concrete type, invisible to every client,
// free to change its fields, its base rate table, its HTTP client,
// without forcing a single client translation unit to recompile.
class HttpCarrierGateway final : public CarrierGateway {
public:
    explicit HttpCarrierGateway(std::string carrier_code) : carrier_code_(std::move(carrier_code)) {}
    long request_rate_quote_cents(double weight_kg) const override {
        return static_cast<long>(weight_kg * 150);
    }
private:
    std::string carrier_code_;
};

std::unique_ptr<CarrierGateway> make_carrier_gateway(std::string_view carrier_code) {
    return std::make_unique<HttpCarrierGateway>(std::string{carrier_code});
}
```

### Procedural interface

Free functions over an opaque handle, the shape C libraries use. A C++ library reaches for it to be callable across a shared-library boundary with no C++ ABI assumptions.

```cpp
// carrier_gateway.h: fully insulating procedural interface. The
// handle's layout is never named here, so an ABI change behind the
// handle never forces a recompile, only a relink.
struct CarrierGatewayHandle;

CarrierGatewayHandle* carrier_gateway_open(const char* carrier_code);
long carrier_gateway_quote_cents(CarrierGatewayHandle* handle, double weight_kg);
void carrier_gateway_close(CarrierGatewayHandle* handle);
```

{% include excalidraw.html name="39-insulation-firewall" caption="Figure G.2: a client depends only on a stable interface; the hidden implementation behind the firewall changes without forcing a recompile on the far side." %}

| Technique | Header exposes | Per-call cost |
| --- | --- | --- |
| PImpl | The class name and public signatures | Heap allocation and a pointer indirection |
| Protocol class | Only the interface and a factory | Virtual dispatch |
| Procedural interface | Free functions and an incomplete handle type | A function call through the handle |

The encapsulated-only version has none of these costs; the compiler sees the real type and often inlines through it. Insulate at a boundary that changes independently of its clients: a public library's ABI, a plugin boundary, a prebuilt binary shipped to a team that can't recompile on demand. Inside code that always builds together, a private member is the right amount of hiding.

## Reducing compile-time coupling

Short of full insulation, the common win is a forward declaration in place of an `#include`, wherever a header needs only a pointer or reference to a type.

```cpp
// Before: manifest.h includes carrier.h just to hold a pointer,
// dragging carrier.h's full transitive include set into every file
// that includes manifest.h.
#include "carrier.h"
#include <string>
#include <vector>

class Manifest {
public:
    void assign_carrier(Carrier* carrier) { carrier_ = carrier; }
private:
    Carrier* carrier_ = nullptr;
    std::vector<std::string> tracking_ids_;
};
```

```cpp
// After: manifest.h forward-declares Carrier, and the real include
// moves to manifest.cpp where assign_carrier's body is defined.
// manifest.h
#include <string>
#include <vector>

class Carrier;  // forward declaration only

class Manifest {
public:
    void assign_carrier(Carrier* carrier);
private:
    Carrier* carrier_ = nullptr;
    std::vector<std::string> tracking_ids_;
};

// manifest.cpp
#include "manifest.h"
#include "carrier.h"

void Manifest::assign_carrier(Carrier* carrier) { carrier_ = carrier; }
```

| Practice | Rule |
| --- | --- |
| Include what you use | A file includes the headers its own declarations need, never relying on a transitive include. A transitive include disappears when its provider stops needing it, and the build breaks somewhere unrelated. |
| Avoid transitive-include bloat | A small header that pulls in a heavy one makes every includer pay the heavy header's parse cost, multiplied by every translation unit. |
| Know where a forward declaration stops | It can't back a value member or a base class, which need the complete type for size and layout, and it can't support a method call. Those need the real include in the header. |

## When this is overkill

Lakos's hierarchy continues above the component: components into packages, packages into package groups, package groups into units of release, down to three-letter package prefixes that keep symbols unique across an enterprise. A codebase with one binary, one team and no shared libraries crossing an organizational boundary gets its benefit from components and levelization. The package-group structure coordinates teams and release schedules that such a project doesn't have.

## For deeper coverage

- John Lakos, [*Large-Scale C++ Volume I: Process and Architecture*](https://www.informit.com/store/large-scale-c-plus-plus-volume-i-process-and-architecture-9780201717068) (Addison-Wesley, 2019): the source for the component, levelization, and insulation discipline reviewed in this appendix. Volume II was never published; implementation-level detail beyond what this appendix covers is not attributable to Lakos's published work.

- The full reading list behind these opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
