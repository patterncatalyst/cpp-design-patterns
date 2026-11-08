---
title: "Template Method and the non-virtual interface"
order: 9
part: "selecting-varying-behavior"
description: "A fixed algorithm skeleton that calls private virtual hooks for the steps that vary, taught against an order export pipeline that produces a CSV manifest, newline-delimited JSON, and a fixed-width EDI feed from the same five-step sequence, with the non-virtual interface idiom protecting the sequence itself from every subclass."
duration: "30 min"
---


## Learning objectives

By the end of this chapter you can:

- State the force Template Method answers: a fixed sequence of steps, some of which vary, with the sequence out of reach of every variant.
- Build a Template Method as a public, non-virtual method that calls private virtual hooks, the Non-Virtual Interface (NVI) idiom.
- Tell a hook from an invariant: a step a subclass overrides versus a precondition or postcondition the base class enforces for every subclass.
- Resolve a hook at compile time, first with the Curiously Recurring Template Pattern, then with deducing this, new in C++23.
- Recognize when Template Method's base class does what one free function already does, and say what to write instead.

## Why this matters

A warehouse hands each order to three downstream systems: one wants a comma-separated manifest, one wants newline-delimited JSON, one wants the fixed-width feed a trading partner's old EDI parser expects. The sequence never changes: validate the order, write a header, write one line per item, write a footer, append a trailer with the format and unit count. Three of the five steps differ per format. The order of the five must not.

Template Method and Strategy solve neighboring problems. Strategy swaps a whole algorithm for another of the same shape. Template Method keeps one algorithm and varies a few of its steps while the rest, including their order, stays fixed.

{% include excalidraw.html name="09-template-method-skeleton" caption="Figure 9.1: OrderExportPipeline's fixed skeleton calls three private virtual hooks in a fixed order; CsvManifestExporter and JsonManifestExporter override the hooks without touching the sequence, validate() and trailer_for() either." %}

## Pattern: Template Method

### The problem

An `Order` carries an id, a customer name and a list of `OrderLine` values, each a SKU and a confirmed quantity:

```cpp
struct OrderLine {
    std::string sku;
    int quantity;
};

struct Order {
    std::string order_id;
    std::string customer_name;
    std::vector<OrderLine> lines;
};
```

Write three exporters as three independent functions and each one carries its own copy of the sequence. One forgets to reject an empty order, another computes the trailer's unit count its own way, and a fix to the sequence has to land three times. Make the entry point virtual and a subclass can override the whole thing: reorder the steps, skip the check, return before the trailer.

### The solution

A **Template Method** defines an algorithm's fixed sequence in one base-class method and leaves the steps that vary to methods a subclass overrides.

The C++ refinement is the **Non-Virtual Interface** idiom, two rules applied together:

- the public entry point is never virtual, so no subclass can touch the sequence;
- the varying steps are private virtual methods, so a subclass supplies their content but can't call them.

### How C++ models it

`OrderExportPipeline::export_order()` is the only method in the hierarchy that is both public and non-virtual:

```cpp
class OrderExportPipeline {
public:
    virtual ~OrderExportPipeline() = default;
    std::string export_order(const Order& order) const;

private:
    static void validate(const Order& order);
    std::string trailer_for(const Order& order) const;

    virtual std::string_view format_name() const = 0;
    virtual std::string render_header(const Order& order) const = 0;
    virtual std::string render_line(const OrderLine& line) const = 0;
    virtual std::string render_footer(const Order& order) const = 0;
};
```

Its body is the whole algorithm:

```cpp
std::string OrderExportPipeline::export_order(const Order& order) const {
    validate(order);

    std::string document = render_header(order);
    for (const OrderLine& line : order.lines) {
        document += render_line(line);
    }
    document += render_footer(order);
    document += trailer_for(order);
    return document;
}
```

Any caller holding a `CsvManifestExporter`, `JsonManifestExporter` or `FixedWidthEdiExporter` through the base class gets the same five steps in the same order. Only the text from three of them changes.

### The hooks: private, not protected

Each concrete exporter overrides the hooks and nothing else:

```cpp
class CsvManifestExporter : public OrderExportPipeline {
private:
    std::string_view format_name() const override;
    std::string render_header(const Order& order) const override;
    std::string render_line(const OrderLine& line) const override;
    std::string render_footer(const Order& order) const override;
};
```

| Exporter | Header | Per line | Footer |
| --- | --- | --- | --- |
| `CsvManifestExporter` | Two lines naming the order and the columns | One comma-separated row | Empty |
| `JsonManifestExporter` | One JSON object for the order | One JSON object, no wrapping array | Empty |
| `FixedWidthEdiExporter` | `ORD` segment, padded columns | `LIN` segment | `END` segment with the line count |

Newline-delimited JSON suits a consumer that reads one line at a time, and it needs no comma bookkeeping between elements.

`protected` would let a subclass call `render_header()` from its constructor or any other method, producing a fragment that never went through `export_order()`. `private` blocks the call and still permits the override, because C++ checks access where a function is called, inside the base class, and not where an override is declared. A derived class that calls `render_header()` fails to compile with `'render_header' is a private member of 'orderexport::CsvManifestExporter'`.

### Invariants a hook can't touch

Two of the five steps aren't hooks:

- **`validate()`** is a static, non-virtual precondition. An order with no lines is rejected with `std::invalid_argument` before any hook runs, whatever the format.
- **`trailer_for()`** is the non-virtual postcondition. It appends the trailer after `render_footer()` returns, with the unit count from `total_quantity()` in `order_domain.cpp`, the one place an order's units are summed.

A subclass whose `render_footer()` returns a fixed string and ignores the order still ends a four-unit document with `-- 4 unit(s) exported via` and its format name, because the base class computes that number. The hooks run in a fixed order, `render_header`, `render_line` once per line, `render_footer`, then `format_name` from inside `trailer_for()`, and an empty order throws with none of them called.

Making every step virtual looks more flexible, until a subclass uses that flexibility to break an invariant the design relied on.

### Compile-time hooks: CRTP or deducing this

Everything above resolves its hooks through a vtable: one indirect call per hook, in exchange for a format chosen after the program starts. `src/static_dispatch_variant.hpp` resolves a single hook at compile time instead. It doesn't replace `OrderExportPipeline`; an exporter with three hooks and a precondition keeps the virtual design. It covers the narrower case of one hook with no precondition to protect.

The C++20 form is the Curiously Recurring Template Pattern (CRTP): a base class template takes its derived type as a template argument, so it can cast `*this` down and call a method it never declared. `CrtpOrderSummary` derives from `CrtpSummarySkeleton<CrtpOrderSummary>`, naming itself in its own base, and the base pays for that with a `static_cast`.

Deducing this, new in C++23, removes both. `summarize()` takes an explicit object parameter, `this const Self& self`, and the compiler deduces `Self` as the calling type:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: CRTP. The base class must be a template parameterized on the
// derived type, and the one step that needs that type casts *this down
// to it explicitly.
template <typename Derived>
class CrtpSummarySkeleton {
public:
    std::string summarize(const Order& order) const {
        return static_cast<const Derived&>(*this).describe(order);
    }
};
```
```cpp
// C++23: deducing this. The base class is an ordinary, non-template
// class; summarize() takes an explicit object parameter, and the
// compiler deduces Self as the most-derived type at the call site, no
// CRTP template argument and no static_cast anywhere.
class DeducingThisSummarySkeleton {
public:
    template <typename Self>
    std::string summarize(this const Self& self, const Order& order) {
        return self.describe(order);
    }
};
```

`DeducedOrderSummary` inherits from `DeducingThisSummarySkeleton` like any ordinary base. Both forms resolve the call at compile time with no vtable.

## When not to use it

- **One format.** With no second format in hand or coming, write a function that validates an order and writes CSV. A virtual base class adds a vtable, a `private`/`virtual` split to learn and another file, to protect a sequence that has nothing to vary against.
- **A sequence that changes per caller.** If a caller needs only the header and footer, or must interleave other work between steps, one `export_order()` fights the problem. Compose free functions, or one `std::function` per step, in the order each caller needs.
- **Compile-time hooks without a measured reason.** CRTP and deducing this answer one hook, no precondition and a measured reason to avoid a vtable. Otherwise a plain virtual hook reads start to finish without learning either idiom.

> **🤖 Explore with your agent**
>
> "In `examples/09-template-method`, add `XmlManifestExporter` deriving from `OrderExportPipeline`. Override only `format_name()`, `render_header()`, `render_line()` and `render_footer()`, and keep all four `private`. Don't make `export_order()` virtual, don't touch `validate()` or `trailer_for()`, and don't sum quantities in the new class; the trailer's unit count comes from `total_quantity()`. Add tests that an empty order throws `std::invalid_argument` and that the document ends with the trailer."
>
> Run `ctest --preset cpp23` and check that no `virtual` or `protected` was added to `OrderExportPipeline`.

## Build, run, observe

From the repository root, the superbuild picks up `examples/09-template-method`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 10 cases pass under `cpp23` and 8 under `cpp20`; the two deducing-this cases compile only under C++23. To see the three formats export one order:

```sh
examples/09-template-method/demo.sh cpp23
```

It exports a two-line order for Riverside Hardware Co. in each format, then prints the CRTP and deducing-this summaries:

```
== CSV manifest (CsvManifestExporter) ==
order_id,customer_name
ORD-77,Riverside Hardware Co.
sku,quantity
SKU-100,3
SKU-205,1
-- 4 unit(s) exported via CSV --

== Newline-delimited JSON (JsonManifestExporter) ==
{"order_id":"ORD-77","customer_name":"Riverside Hardware Co."}
{"sku":"SKU-100","quantity":3}
{"sku":"SKU-205","quantity":1}
-- 4 unit(s) exported via JSON --

== Fixed-width EDI (FixedWidthEdiExporter) ==
ORDORD-77      Riverside Hardware Co.  
LINSKU-100     00003
LINSKU-205     00001
END00002
-- 4 unit(s) exported via fixed-width EDI --

== Static dispatch: CRTP (C++20) ==
ORD-77: 2 line(s)

== Static dispatch: deducing this (C++23) ==
ORD-77: 2 line(s)
```

Every trailer reports four units and both summaries report two lines. Under `cpp20` the output stops before the deducing-this block.

- **Validate after the header.** In a scratch copy, move `validate(order)` below the `render_header()` call. One case fails: an empty order now reaches a hook before it's rejected.
- **Call a hook from a subclass.** Add a member to a class derived from `CsvManifestExporter` that calls `render_header()`. The build fails with `'render_header' is a private member of 'orderexport::CsvManifestExporter'`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Template Method entry, including the "hook" and "fixed sequence" framing `OrderExportPipeline` builds on.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/09-template-method`, which asserts the hook-call order through a recording subclass.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
