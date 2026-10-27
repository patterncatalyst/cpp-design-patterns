---
title: "Large-scale C++: physical design, reviewed in code"
order: 39
layout: tutorial
sectionid: docs
part: appendices
appendix: "G"
description: "A code-first review of John Lakos's physical-design discipline for C++, the component, levelization, and insulation, on the book's shipping domain."
---

## How to read this appendix

Every principle and pattern in this book so far has been about logical design: which class does what, which interface a client programs against, which object collaborates with which. Physical design asks a different question: which file includes which other file, and what does that cost at compile time, at link time, and in the test matrix. The answer only matters because of four facts specific to C++. Separate compilation means a `.cpp` file is translated on its own, so every fact it needs from elsewhere arrives through a header it includes. Header inclusion is textual: `#include` is a copy-paste before the compiler ever sees a type. The one-definition rule requires every translation unit that sees a type's definition to see the identical definition, so a header that two files include differently is a silent bug waiting on a build machine. And the link model resolves symbols after every translation unit has been compiled separately, so a cycle between two headers is not caught by the compiler that reads either one, only by the linker, or by a test harness that discovers it cannot build one component without the other.

Java, Python, and C# do not have this set of forces. A Java class file carries its own metadata and a module system resolves dependencies without re-parsing source text; Python imports a module once and caches it; C# assemblies are linked against metadata, not textual headers. None of the three forces, a header, a one-definition rule, or a build-time link step, exists in the way C++ has all three at once. The vocabulary in this appendix, component, levelization, insulation, is C++-specific for that reason. It has no sibling-language equivalent because no sibling language generates the failure modes it answers.

The source for all three is John Lakos's *Large-Scale C++ Volume I: Process and Architecture*. Lakos distinguishes physical design from logical design as sharply as this book distinguishes a pattern from a principle: logical design is what a UML diagram shows, and physical design is what the build system has to do to turn source into a working program. A design can be logically correct, every class well-named, every responsibility singular, and still be physically unbuildable in isolation, untestable component by component, and expensive to change because one header touches half the codebase. The sections below follow Lakos's spine: the component as the unit of physical design, the include graph and the levelization discipline that keeps it a DAG, insulation as a stronger guarantee than encapsulation, and the smaller habits that keep compile-time coupling down day to day.

## The component

A component is one header and its matching source file, treated as a single, indivisible unit of physical design. `rate_quote.h` and `rate_quote.cpp` are a component. The pairing matters because a component is also the unit of testing, the unit of reuse, and the unit a build system compiles, links, and ships independently of its neighbors. A class is a logical idea; a component is where that idea lives on disk, and the two should correspond one to one for exactly one class, plus the small helpers that class cannot function without.

Lakos states four rules for what makes a pair of files a well-formed component, and all four are checkable without reading a single line of implementation.

**Logical and physical name cohesion.** The component's file name and its primary class name derive from each other. `carrier_gateway.h` declares `CarrierGateway`. A reader who knows the class name can guess the file name, and a reader scanning file names in a directory listing already knows what each one declares. A header named `shipping_utils.h` that happens to declare `Invoice` breaks this on sight.

**A component includes its own header first.** The convention, enforced by eye or by a lint rule, is that `carrier_gateway.cpp`'s first substantive `#include` is `"carrier_gateway.h"`, before any system header, before any other project header. This is not a style preference; it is a self-sufficiency proof. If `carrier_gateway.h` secretly depends on `<string>` having already been included by something earlier in the file, putting it first is exactly what exposes that: the header fails to compile on its own, which is the bug the convention exists to catch before a client discovers it by including `carrier_gateway.h` in a different order and getting a wall of unrelated errors.

**One primary class per component.** A component may declare small, tightly coupled helper types the primary class cannot be used without, an iterator, a small value type returned by one of its methods, but it does not declare a second, independently useful class. Two independently useful classes in one component cannot be tested, reused, or linked separately, which defeats the reason components exist.

**One test driver per component.** Lakos's convention names this file `<component>.t.cpp`, for example `carrier_gateway.t.cpp`. This book's examples use GoogleTest fixtures under `examples/`, which is the same discipline under different file-naming machinery: exactly one test binary's worth of cases exists for exactly one component, so a change to `CarrierGateway` has one place to add a case and one test target to run while iterating on it.

The before-and-after below shows what these four rules protect against.

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

`invoice.h`/`invoice.cpp` and `manifest.h`/`manifest.cpp` follow the identical shape, each with its own `.t.cpp` driver. A client that needs only `RateQuote` compiles and links only `rate_quote.cpp`; a client that needs all three pays for all three, by choice, not by accident of one shared header.

## Physical dependencies and levelization

Component A physically depends on component B when A cannot compile, link, or run its tests without B present. This is a stricter, more measurable claim than "A uses B logically." A physical dependency shows up as an `#include`, as a symbol resolved at link time, or as a test fixture that has to construct a real B because no seam exists to substitute one.

Draw every component as a node and every physical dependency as a directed edge, and the result is the include graph. Lakos's central claim about this graph is that it must be a directed acyclic graph. A cycle, A depends on B and B depends on A, means neither component can be compiled, linked, or tested without the other already existing, which makes "without the other" a meaningless phrase: there is no smaller unit left to test in isolation. A cycle does not merely complicate the build, it erases the testability and independent-reuse properties that components exist to provide.

Where the graph is acyclic, Lakos assigns level numbers. A component that depends on nothing else within its group is level 1. A component whose heaviest dependency is level N−1 is level N. The practical payoff is a testing order: level 1 components are tested first, with no fakes required for anything below them because nothing is below them; level 2 components are tested next, against real level-1 dependencies that are already known-good; and so on up the graph. A codebase with well-defined levels has a build and test order a script can compute from the include graph alone.

A `Shipment` that needs its assigned `Carrier`'s service area to validate a destination, and a `Carrier` that needs a shipment's weight and dimensions to quote a rate, is a natural place for a cycle to form: each class seems to legitimately need a fact that lives on the other. Lakos names several techniques for breaking exactly this shape. The three most directly applicable to a `Shipment`/`Carrier` pair are below.

**Escalation.** Move the operation that needs both types up into a new, higher-level component that depends on both peers, instead of letting either peer depend on the other.

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

bool is_valid_for(const Shipment& shipment, const Carrier& carrier) {
    return carrier.services_zone(shipment.destination_zone());
}

long quote_rate_cents(const Carrier& carrier, const Shipment& shipment) {
    return carrier.base_rate_cents(shipment.weight_kg());
}
```

**Demotion.** When the mutual dependency is really about one shared fact, move that fact down into a new, lower-level component both peers can depend on without depending on each other.

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

**Opaque pointers.** When one peer only ever holds a pointer or reference to the other, forward-declare it instead of including its header, and move the include into the `.cpp` where the pointer is dereferenced.

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

{% include excalidraw.html name="39-levelization-before-after" caption="Figure G.1: Shipment and Carrier as a cyclic include graph, redrawn as a levelized DAG after escalating the shared operation into a manager-class coordinator." %}

A manager class is the technique Figure G.1 draws directly: a new `ShipmentCarrierCoordinator` at level 2 owns the operation that needs both peers, and `Shipment` and `Carrier` at level 1 never name each other. Escalation and the manager class overlap; the manager class is escalation specifically into a stateful owning component rather than a stateless free function, useful once the coordinating logic needs to hold its own data, a cache of recent quotes, for example, rather than just compute from its two arguments.

A fifth technique, dumb data, does not fit the `Shipment`/`Carrier` pair as cleanly but belongs in the same list: one component carries a value the other interprets without either depending on the other's type. A `TrackingRecord` that stores a carrier-specific status code as a plain integer, leaving `Carrier` to interpret what the integer means, is dumb data. `TrackingRecord` never needs to know `Carrier` exists to store the code; only the component that interprets it needs to depend on `Carrier`.

The tension across every technique here is the same: breaking a cycle always adds a component, a forward declaration, or an indirection that was not there before. A codebase with one real two-way relationship and no plan to grow past it can tolerate a documented, intentional merge of the two types into one component instead of paying for three files to express what one file could. Levelization earns its cost once a third component needs one of the two peers without wanting the other, which a cyclic pair cannot support no matter how the cycle is drawn.

## Insulation versus encapsulation

These two words are often used as if they mean the same thing, and Lakos is precise about why they do not. Encapsulation hides an implementation detail from logical access: a private data member cannot be read or written by a client's code, so no client can depend on it for correctness. But a private data member still sits inside the class definition, inside the header, where every client includes it. Add a field, remove a field, or change a field's type, and every translation unit that includes that header has to recompile, even though no client's logic touches the field directly. The member was encapsulated. It was never insulated.

Insulation is a stronger guarantee: a change to an implementation detail does not force a client to recompile at all. The detail does not merely sit behind a private access specifier, it sits in a different file, reachable only through a layer that does not change when the detail does. Insulation costs something encapsulation does not: an indirection, a heap allocation for a hidden object, or a virtual call through an abstract interface, paid on every use in exchange for a compile that the hidden change never triggers.

This book already builds the PImpl, handle-body idiom in full elsewhere: a class exposes a stable header containing only a pointer to a forward-declared implementation class, and that class's definition, along with every private member and private method it holds, lives entirely in the `.cpp`. In Lakos's taxonomy, PImpl is exactly one insulation technique among several, the one that keeps the stable type's identity (its name, its public method signatures) in the header while hiding everything about how it stores and computes.

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

A second technique reaches further: a protocol class is a pure abstract base class that declares only public virtual methods and a virtual destructor, with no data members at all. The header carries the interface; a concrete class implementing it lives entirely in a `.cpp`, reachable only through a factory function the header also declares.

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

A third technique drops the class entirely: a procedural interface exposes free functions over an opaque handle, usually a `void*` or an incomplete struct pointer, the shape C libraries use and the shape a C++ library reaches for when it needs to be callable across a shared-library boundary with no C++ ABI assumptions at all.

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

The shared cost across all three techniques is in Figure G.2: a heap allocation for the hidden object, a virtual dispatch through the protocol class's vtable, or a function-call indirection through the opaque handle, every time a client calls through the firewall. None of that cost shows up in the encapsulated-but-not-insulated version, where the compiler can see the real type and often inline straight through it. Insulation is worth paying for at a boundary that changes independently of its clients: a public library's ABI, a plugin boundary, a component shipped as a prebuilt binary to a team that cannot recompile it on demand. It is a poor trade inside a single compilation unit's worth of code that always builds together; there, the private data member that is encapsulated but not insulated is the right amount of hiding, and the extra indirection buys nothing a client ever needed.

## Reducing compile-time coupling

Short of a full insulation technique, a handful of smaller habits keep a header's physical footprint close to what it logically needs. The most common win is a forward declaration in place of an `#include`, whenever a header only needs a pointer or a reference to a type, never its size, its layout, or a value member of that type.

```cpp
// Before: manifest.h includes carrier.h and <vector> just to hold a
// pointer and a count, dragging both headers' full transitive
// include sets into every file that includes manifest.h.
#include "carrier.h"
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

The practice this supports is include-what-you-use: a header or source file includes exactly the headers its own declarations need, never a header it only happens to receive transitively through some other include. A file relying on a transitive include breaks the moment the component providing that include decides it no longer needs it, which is an invisible dependency until the build fails somewhere unrelated. The companion failure mode is transitive-include bloat: a small header that includes one heavy header, `<vector>` dragged in by a type that is only ever passed by pointer, for instance, forces every file that includes the small header to pay the heavy header's full parse cost, multiplied by however many translation units include it.

A forward declaration has a real limit. It cannot back a value member, because the compiler needs the complete type to know the member's size and alignment. It cannot back a base class, for the same reason, and it cannot support calling any method on the forward-declared type, only naming a pointer or reference to it. The moment any of those needs arrives, the real include has to come back, in the header this time, because the information belongs there.

## When this is overkill

Lakos's full hierarchy extends well above the component: components group into packages, packages into package groups, and package groups into units of release, each with its own naming convention, down to the three-letter package prefix that keeps symbols link-time-unique across an entire enterprise codebase. A small codebase, one binary, one team, no shared libraries crossing an organizational boundary, gets the real benefit from the component and levelization discipline in this appendix and has little use for the package-group ceremony above it; that structure exists to coordinate dependencies across teams and release schedules that a single-binary project does not have.

## For deeper coverage

- John Lakos, [*Large-Scale C++ Volume I: Process and Architecture*](https://www.informit.com/store/large-scale-c-plus-plus-volume-i-process-and-architecture-9780201717068) (Addison-Wesley, 2019): the source for the component, levelization, and insulation discipline reviewed in this appendix. Volume II was never published; implementation-level detail beyond what this appendix covers is not attributable to Lakos's published work.

- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
