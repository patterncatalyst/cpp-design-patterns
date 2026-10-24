---
title: "Template Method and the non-virtual interface"
order: 9
part: "selecting-varying-behavior"
description: "A fixed algorithm skeleton that calls private virtual hooks for the steps that vary, taught against an order export pipeline that produces a CSV manifest, newline-delimited JSON, and a fixed-width EDI feed from the same five-step sequence, with the non-virtual interface idiom protecting the sequence itself from every subclass."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- State the force Template Method answers: an algorithm's sequence of steps is fixed, but some of those steps vary by context, and the sequence itself must stay out of reach of every variant.
- Build a Template Method as a public, non-virtual method that calls private virtual hooks, and explain why that combination, not virtual dispatch alone, is the Non-Virtual Interface (NVI) idiom.
- Tell a hook from an invariant: a step a subclass overrides versus a precondition or postcondition the base class enforces for every subclass without exception.
- Contrast a hook resolved at runtime through a vtable with the same hook resolved at compile time, first through C++20's Curiously Recurring Template Pattern, then through C++23's deducing this.
- Read a GoogleTest suite that asserts a fixed call order directly, not just a finished document that happens to look right.
- Recognize when Template Method's base class is solving a problem a single free function already solves, and say what to write instead.

## Why this matters

A warehouse system has to hand an order off to three downstream systems that each want something different: one wants a comma-separated manifest, one wants newline-delimited JSON, one wants a fixed-width feed a trading partner's twenty-year-old EDI parser still expects. The sequence that produces any one of those documents never changes. Validate the order. Write a header. Write one line per item. Write a footer. Append a trailer naming the format and the unit count. Three downstream systems, one sequence, three different outputs for exactly three of its five steps.

That is the shape Chapter 1 called a force: the sequence is fixed, the content of a few steps inside it varies, and the two must not be allowed to mix. Template Method is the Gang of Four's name for resolving it with inheritance: a base class owns the sequence in one method, and each step that varies becomes a separate method a subclass overrides. The idea sounds close to Chapter 8's Strategy, and the two do solve related forces, but they solve different halves of the same problem. Strategy swaps an entire algorithm for another one of the same shape, carrier A's pricing rule for carrier B's. Template Method keeps one algorithm and varies a handful of its steps while the rest, including the order the steps run in, never moves. A checkout choosing a pricing algorithm wants Strategy; an export pipeline whose five steps always run in the same order, with three of them producing different text per format, wants Template Method.

The refinement this book's C++ examples add to the Gang of Four's shape is the Non-Virtual Interface idiom. A generated version of Template Method, the one you get from describing it to an LLM without more context, usually makes the entry point itself virtual and lets a subclass override it directly. That throws away the one guarantee Template Method exists to provide: nothing stops a subclass from overriding the whole sequence, reordering steps, skipping the precondition, or returning before the trailer runs. NVI closes that gap with two rules applied together. The public entry point is never virtual, so no subclass can touch the sequence. The steps that do vary are private virtual methods, called only from inside the base class, so a subclass supplies their content but has no way to call them directly, and nothing outside the base class can reach them at all.

{% include excalidraw.html name="09-template-method-skeleton" caption="Figure 9.1: OrderExportPipeline's fixed skeleton calls three private virtual hooks in a fixed order; CsvManifestExporter and JsonManifestExporter override the hooks without touching the sequence, validate() and trailer_for() either." %}

## The domain: exporting an order three ways

The example behind this chapter lives in `examples/09-template-method`. `Order` carries an id, a customer name, and a list of `OrderLine` values, each a SKU and a quantity a warehouse picker already confirmed:

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

`total_quantity()` in `src/order_domain.cpp` is the one place an order's units get summed, the same shared-core move Chapter 8 used for its three pricing strategies: every exporter's trailer calls this one function, so the unit count a trailer reports can never drift from the order's actual lines, no matter which format produced the body above it.

## The skeleton: one public, non-virtual method

`OrderExportPipeline::export_order()` is the entry point every caller uses, and it is the only method in the class hierarchy that is both public and non-virtual:

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

Its body is the whole algorithm, five steps, always in this order:

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

A caller holding any concrete exporter through this base class, a `CsvManifestExporter`, a `JsonManifestExporter`, a `FixedWidthEdiExporter`, gets the exact same five-step sequence every time. The only thing that changes between them is the text three of those steps produce. That is Template Method's whole claim: the algorithm's shape lives in exactly one method, written once, and every variant inherits that shape rather than re-implementing it.

## The hooks: private, not protected

`render_header()`, `render_line()`, and `render_footer()` are the three steps that vary, and each concrete exporter overrides all three:

```cpp
class CsvManifestExporter : public OrderExportPipeline {
private:
    std::string_view format_name() const override;
    std::string render_header(const Order& order) const override;
    std::string render_line(const OrderLine& line) const override;
    std::string render_footer(const Order& order) const override;
};
```

`CsvManifestExporter` writes a two-line header naming the order, then one comma-separated row per line item. `JsonManifestExporter` writes newline-delimited JSON instead, one object per line with no wrapping array and no comma bookkeeping between elements, the practical choice when a downstream consumer reads the feed one line at a time rather than parsing one giant array. `FixedWidthEdiExporter` writes fixed-width segments, `ORD` for the header, `LIN` per line, `END` for the footer, each column padded to the width a legacy trading-partner parser expects.

Declaring the hooks `private` rather than `protected` matters, because most inheritance tutorials reach for `protected` by reflex whenever a derived class needs to override something. `protected` would let a subclass call `render_header()` directly, from its own constructor, from an unrelated method, from anywhere, and once that is possible a subclass can produce a document fragment that was never run through `export_order()`'s sequence at all. `private` closes that door while still permitting the override: C++ lets a derived class override a private virtual function it has no access to call, because access control is checked at the call site inside the base class, not at the point where the override is declared. The result is a hook a subclass can fill in but never invoke, which is exactly the capability NVI needs and nothing more.

## Invariants a hook cannot touch

Two of `export_order()`'s five steps are not hooks at all, and the distinction matters as much as the hooks themselves. `validate()` is a static, non-virtual precondition: an order with no line items is a programming error at this boundary regardless of which format was asked for, so it is checked once, in the base class, before any hook runs. `trailer_for()` is the matching postcondition, a non-virtual method that appends a trailer line after `render_footer()` returns, no matter what `render_footer()` did. Neither method takes a virtual call anywhere in its path, and neither can be overridden, skipped, or reordered by a concrete exporter.

This is the half of Template Method a generated base class most often gets wrong, because making every step virtual looks more flexible, right up until a subclass uses that flexibility to break an invariant the design depended on. `examples/09-template-method`'s test suite proves the distinction directly with a `RecordingExporter`, a test-only subclass that logs every hook call into a vector and overrides `render_footer()` to produce a fixed string with no reference to the order's unit count at all:

```cpp
TEST(OrderExportPipelineTest, AppendsTheTrailerRegardlessOfWhatRenderFooterProduced) {
    RecordingExporter exporter;

    const std::string document = exporter.export_order(sample_order());

    EXPECT_TRUE(document.ends_with("-- 4 unit(s) exported via RECORDING --\n")) << document;
}
```

The trailer still reports the correct unit count, four, computed from `total_quantity()` in the base class, even though the hook that ran just before it never touched that number. A second test asserts the call order itself, not just the finished text, by reading the same `RecordingExporter`'s log after a call to `export_order()`:

```cpp
TEST(OrderExportPipelineTest, CallsHooksInTheFixedSkeletonOrder) {
    RecordingExporter exporter;
    exporter.export_order(sample_order());

    const std::vector<std::string> expected = {
        "render_header", "render_line:SKU-100", "render_line:SKU-205",
        "render_footer", "format_name",
    };
    EXPECT_EQ(exporter.calls, expected);
}
```

`format_name` shows up last because `trailer_for()` calls it, after `render_footer()`, which is the order the production code runs in rather than an order a reader has to infer from reading `export_order()` a second time. A third test confirms `validate()` runs before any hook does at all: given an order with no line items, `RecordingExporter`'s call log stays empty, because the thrown exception unwinds out of `export_order()` before `render_header()` is ever reached.

## A narrower contrast: CRTP versus deducing this

Everything above resolves its three hooks at runtime, through a vtable, the same tradeoff Chapter 8 named for Strategy's first expression: one indirect call per hook, paid in exchange for a format that can be chosen after the program starts. A narrower illustration in `src/static_dispatch_variant.hpp` shows what changes when one hook is resolved at compile time instead, first the C++20 way, then the C++23 way. Neither of the two classes here replaces `OrderExportPipeline`; a format exporter with three hooks and a precondition to enforce keeps the virtual design above. The contrast exists for a narrower case, a single hook with no precondition to protect, where a reviewer has to choose between a long-standing idiom and a newer one that removes its characteristic awkwardness.

The C++20 way is the Curiously Recurring Template Pattern, CRTP: a base class template that takes its own derived type as a template argument so it can cast `*this` down to that type and call a method the base class never declared.

```cpp
template <typename Derived>
class CrtpSummarySkeleton {
public:
    std::string summarize(const Order& order) const {
        return static_cast<const Derived&>(*this).describe(order);
    }
};

class CrtpOrderSummary : public CrtpSummarySkeleton<CrtpOrderSummary> {
public:
    std::string describe(const Order& order) const {
        return order.order_id + ": " + std::to_string(order.lines.size()) + " line(s)";
    }
};
```

`CrtpOrderSummary` has to name itself as the template argument to its own base class, a construction every C++ programmer eventually has to have explained to them once, and the `static_cast` inside `summarize()` is the price of a base class that is a template at all. Both of those are accepted costs in codebases that reach for CRTP, but neither is self-explanatory to a reader meeting the pattern for the first time.

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

`DeducingThisSummarySkeleton` is not a template, and `DeducedOrderSummary` inherits from it the same way it would inherit from any ordinary base class, naming nothing about itself in the inheritance list. `summarize()`'s first parameter, `this const Self& self`, is C++23's explicit object parameter: the compiler deduces `Self` from whatever concrete type calls `summarize()`, the same information CRTP's template argument carried, without asking the class hierarchy to carry a template parameter to get it. Both versions compile to the same zero-overhead call, resolved entirely at compile time with no vtable anywhere in either one; the only thing that changed between the two columns is which C++ idiom gets the derived type into the base class's hands.

> **AI sidebar.** Ask an LLM pairing partner for a Template Method base class and a common result makes the entry point virtual too, because that is the shape that appears most often in tutorials that teach "override this method" as the whole idea without naming the sequence it protects. Check the result the way this chapter trained: is there exactly one non-virtual public entry point, and are the varying steps private, not protected, so a subclass can supply them but never call them directly from somewhere else in its own code? A generated hierarchy that makes `export_order()` itself overridable has reintroduced the exact failure mode NVI exists to close, a subclass free to skip the precondition or run the steps out of order, and it will pass every demo that only ever calls the base class's own sequence. The gap only shows up once someone writes a subclass that does something clever with the override, which is precisely the subclass this design is supposed to make impossible to write.

## When not to use Template Method

Template Method earns its base class when a multi-step sequence is shared by more than one concrete type and the sequence itself, not just one step in isolation, is the thing worth protecting from drift. It stops paying for itself quickly outside that case.

A single format with no second one in hand or clearly coming does not need `OrderExportPipeline`, `CsvManifestExporter`, and a one-class hierarchy under it; it needs a function that validates an order and writes CSV, the same candidate-non-pattern move Chapter 1 trained for every pattern in this book. Introducing a virtual base class ahead of a second format arriving adds a vtable, a `private`/`virtual` split a reader has to learn, and an extra file, for a design decision, protecting the sequence from variation, that has nothing yet to protect against.

Composition is the stronger choice, over either Template Method or Chapter 8's Strategy, when the steps that vary do not all belong to one coherent sequence a single class should own. If a caller sometimes needs to run only the header and footer without the line loop, or needs to interleave export steps with unrelated work between them, forcing all of that through one `export_order()` method fights the actual shape of the problem. A set of free functions, or a Strategy-shaped `std::function` per step, composed by the caller in whatever order that caller needs, serves a flexible sequence better than a Template Method whose entire premise is a fixed one. The test is the one this book keeps returning to: if the sequence of steps is the fact worth guaranteeing, Template Method is the right name for the base class; if the sequence itself needs to change per caller, the base class is guarding an invariant nobody asked for.

The CRTP-versus-deducing-this contrast matters in an even narrower case than the virtual design above it. A single hook with no precondition to enforce and a real, measured reason to avoid a vtable is the case either idiom answers; absent that reason, a plain virtual hook, resolved at runtime, is the version a reviewer can read start to finish without first learning either idiom at all.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/09-template-method` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 10 cases under the `cpp23` preset, 8 under `cpp20`, the difference being the two deducing-this cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 10   # cpp23
100% tests passed, 0 tests failed out of 8    # cpp20
```

To see all three formats export the same order side by side rather than just reading an assertion that says each one is correct, run the example's own demo script:

```sh
examples/09-template-method/demo.sh cpp23
```

It exports a two-line order for Riverside Hardware Co. through all three formats, then prints the CRTP and deducing-this summaries of the same order:

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

Notice the trailer on every format agrees on the same unit count, four, and the CRTP and deducing-this summaries agree on the same line count, two. That agreement is `total_quantity()` and each exporter's shared fixed sequence showing up as an observable fact at the terminal, not just as a passing test. Running `examples/09-template-method/demo.sh cpp20` prints the same output minus the final deducing-this block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley): the original Template Method entry, including the "hook" and "fixed sequence" framing this chapter's `OrderExportPipeline` builds on.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/09-template-method`'s suite, which asserts a fixed hook-call order directly through a recording subclass rather than only checking a finished document.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
