---
title: "Composite, Flyweight, and Null Object"
order: 17
part: "composing-adapting-structure"
description: "Three idiomatic ways to keep a recursive structure simple, taught against a palletizing domain: a Composite interface that treats one item and an entire pallet identically, a Flyweight pool that shares a handful of SKU descriptors across thousands of line items, and a Null Object that lets a weight-limit check skip every null branch."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force Composite answers: client code that needs to treat a
  single object and an arbitrarily deep tree of objects through the exact
  same interface, with no case statement telling leaf and branch apart.
- Build a Composite where a leaf and a composite both recurse correctly,
  including a composite nested inside another composite.
- Name the force Flyweight answers: a program holding so many copies of the
  same immutable data that the copies, not the data's logic, become the
  memory problem.
- Build a Flyweight pool that returns one shared instance per key and prove
  the sharing, rather than assume it, with an identity check.
- Build a Null Object and explain why it removes a null check from every
  call site that uses it, not just the one call site that constructs it.
- Choose between a `std::map` and C++23's `std::flat_map` for a small,
  rarely-mutated pool, and say what each one costs.
- Recognize when any of the three is solving a problem a plain class,
  a `std::vector`, or a `nullptr` check already solves, and say what to
  write instead.

## Why this matters

A pallet ready to leave a warehouse is a tree. The pallet holds cartons, a
carton can hold more cartons before it holds a single item, and the
question a shipping system asks at every level is the same one: how much
does this weigh, and how many units does it hold. A naive answer writes one
function for "weigh an item" and a second, nearly identical function for
"weigh a carton," and a third for "weigh a pallet," each one aware of what
kind of thing it is summing. That duplication is not a style complaint. The
moment a carton gets nested inside another carton, which happens the first
time a warehouse consolidates two partial shipments into one box, the
carton-weighing function has to learn to call itself, and now two of the
three functions are doing the same recursive work under different names.
Composite is the Gang of Four's name for refusing that duplication outright:
one interface, implemented by both the leaf and the thing that holds leaves,
so "weigh this" means the same call no matter how deep the tree goes.

A second, unrelated pressure shows up once that pallet actually ships.
A single SKU, a particular battery pack or a particular gasket, might appear
on hundreds of line items across a day's shipments, and every one of those
line items needs the same facts about it: its weight, its dimensions,
whether it is hazardous to ship. Storing that SKU's full description inside
every line item that references it is correct and wasteful in the same
breath; the facts never change between one line item and the next; only the
quantity and the destination do. Flyweight is the Gang of Four's name for
separating those two kinds of state and sharing the part that never
changes, so a thousand line items referencing one SKU share one description
object instead of carrying a thousand copies of it.

A third pressure is smaller but no less common: a carton's weight-limit
check needs somewhere to report a violation, and most of the time nobody
has wired up anything to receive that report yet. The naive fix guards
every call site with a null check: "if there is a handler, tell it." That
guard is cheap to write once and expensive to keep correct everywhere it
gets copied, because every new call site that forgets the check is a crash
waiting on a shipment nobody flagged as overweight. Null Object is the
pattern that removes the check instead of remembering to write it: a
do-nothing implementation stands in for "no handler," so every call site
can call the handler unconditionally.

None of the three forces above are the same force, and the Gang of Four did
not group them in one chapter because they resemble each other
structurally. They are grouped here because a single palletizing domain
needs all three at once, and seeing them solve three different problems in
the same code makes each one's boundary sharper than seeing it
alone would.

{% include excalidraw.html name="17-composite-flyweight" caption="Figure 17.1: a composite tree (pallet, cartons, items) sharing one interface on the left; a flyweight pool of SKU descriptors referenced by many line items on the right." %}

## The domain: palletizing a shipment

The example behind this chapter lives in `examples/17-composite-flyweight`.
A warehouse packs a pallet from cartons, a carton from items or from more
cartons, and each item references a SKU that describes what it actually is:

```cpp
enum class HazmatClass { None, Flammable, Corrosive, Lithium };

struct SkuDescriptor {
    std::string sku_code;
    std::string product_name;
    int unit_weight_grams;
    int length_mm;
    int width_mm;
    int height_mm;
    HazmatClass hazmat_class;
};
```

Nothing in `SkuDescriptor` varies per line item. A `WIDGET-100` weighs the
same 250 grams whether it is line one on pallet nine or line sixty on pallet
twelve. That is the fact Flyweight below leans on: `SkuDescriptor` is safe
to share because nothing about it is specific to any one package.

## Composite: one interface, leaf or branch

`PackageComponent` is the interface both a single item and an entire group
of items implement:

```cpp
class PackageComponent {
public:
    virtual ~PackageComponent() = default;
    virtual int total_weight_grams() const = 0;
    virtual int item_count() const = 0;
    virtual const std::string& label() const = 0;
};
```

`Item` is the leaf. It holds a shared `SkuDescriptor` pointer (more on that
pointer in the next section) plus the two facts that are its own:
how many units this line represents, and where this particular line is
headed.

```cpp
class Item : public PackageComponent {
public:
    Item(std::string label, std::shared_ptr<const SkuDescriptor> sku, int quantity, std::string destination);

    int total_weight_grams() const override;
    int item_count() const override;

private:
    std::shared_ptr<const SkuDescriptor> sku_;  // intrinsic, shared
    int quantity_;                              // extrinsic, per line
    std::string destination_;                   // extrinsic, per line
};
```

`PackageGroup` is the composite, and it is one class, not a separate
`Carton` class and a separate `Pallet` class. A carton and a
pallet are the same thing from `PackageComponent`'s point of view: a
collection of children, queried the same way a leaf is queried. `add()`
accepts any `PackageComponent`, so a `PackageGroup` representing a carton
can hold `Item`s, and a `PackageGroup` representing a pallet can hold other
`PackageGroup`s:

```cpp
class PackageGroup : public PackageComponent {
public:
    explicit PackageGroup(std::string label);

    void add(std::shared_ptr<PackageComponent> child);

    int total_weight_grams() const override;
    int item_count() const override;

private:
    std::vector<std::shared_ptr<PackageComponent>> children_;
};
```

The recursion lives in exactly two functions, and neither one asks what
kind of child it is looking at:

```cpp
int PackageGroup::total_weight_grams() const {
    return std::accumulate(children_.begin(), children_.end(), 0,
                            [](int sum, const std::shared_ptr<PackageComponent>& child) {
                                return sum + child->total_weight_grams();
                            });
}
```

`item_count()` is the same `std::accumulate` call with `item_count()` in
place of `total_weight_grams()`. When a carton holds another carton nested
two levels deep, that inner carton's `total_weight_grams()` call recurses
into its own children the same way, and the outer carton never has to know
the nesting is there. Running the example's demo against a pallet with a
carton that itself holds a nested carton shows the arithmetic working
without any special case: a pallet holding a flat carton and a carton that
wraps a third, deeper carton reports the sum of both branches correctly,
because every branch answered the same two questions the same way.

This is also where Composite pays for itself over the plainer alternative of
a single `std::vector<Item>` with no nesting at all. A flat vector answers
"how much does this pallet weigh" just as easily as a tree does, right up
until a warehouse needs to consolidate two partial cartons into one without
re-flattening every item inside them, or needs to move a whole carton from
one pallet to another as a single unit, or needs the weight of one carton
without summing items it never holds. A flat structure answers the whole
pallet's question but not a sub-tree's question, and this chapter's domain
needs the intermediate-level answer just as much as the top-level one.

## Flyweight: sharing the state that never changes

`SkuCatalog` is both the factory and the pool. It keeps two separate
tables: a registered master catalog of every SKU this warehouse knows how
to describe, and a pool of flyweights actually materialized so far.

```cpp
std::shared_ptr<const SkuDescriptor> SkuCatalog::get(std::string_view sku_code) {
    const std::string key(sku_code);

    if (const auto pooled = flyweights_.find(key); pooled != flyweights_.end()) {
        return pooled->second;
    }

    const auto registered = master_table_.find(key);
    if (registered == master_table_.end()) {
        throw std::out_of_range("unregistered SKU code: " + key);
    }

    auto flyweight = std::make_shared<const SkuDescriptor>(registered->second);
    flyweights_.insert_or_assign(key, flyweight);
    return flyweight;
}
```

The distinction between the two tables is the point. A warehouse's full
product catalog can list thousands of SKUs, most of which never touch any
one pallet; materializing a flyweight for every registered SKU up front
would waste exactly the memory Flyweight exists to save. `get()` only ever
materializes a SKU the first time something actually asks for it, and every
call after that first one returns the same `std::shared_ptr`, which is what
`pool_size()` measures: the number of distinct SKUs this program has
actually shared, not the number of `Item`s that reference them. Building
sixty more line items across the same three SKUs, which the example's demo
does right after building the pallet above, leaves `pool_size()` unchanged
at three; only asking for a fourth SKU would grow it.

Returning a `const SkuDescriptor` by `std::shared_ptr` rather than by value
is what makes the sharing real and checkable rather than assumed. Two
separate `get("WIDGET-100")` calls return pointers that compare equal with
`.get()`, proving the second call reused the first call's heap allocation
instead of quietly making a private copy that merely looks identical. A
Flyweight implementation that returns `SkuDescriptor` by value compiles,
passes a test that checks field equality, and still allocates one
`SkuDescriptor` per `Item`, defeating the entire pattern while looking
correct from the outside. Testing identity, not just equality, is what
catches that.

## Null Object: a do-nothing implementation instead of a null check

`PackageGroup` can report an overweight condition, but most of this
chapter's examples never wire up anything to receive that report. The
naive fix is a null check at the one place `check_weight_limit()` would
call a handler:

```cpp
if (handler_ != nullptr && total > grams_limit) {
    handler_->on_overweight(label_, total - grams_limit);
}
```

That line is correct, and it is also a decision a reviewer has to re-verify
at every call site that touches `handler_`, because nothing in the type
system stops a second method from forgetting the check. Null Object removes
the decision instead of asking every caller to remember it:

```cpp
class ShipmentExceptionHandler {
public:
    virtual ~ShipmentExceptionHandler() = default;
    virtual void on_overweight(const std::string& group_label, int grams_over_limit) const = 0;
};

class NullShipmentExceptionHandler : public ShipmentExceptionHandler {
public:
    void on_overweight(const std::string&, int) const override {}
};
```

`PackageGroup` binds a `const ShipmentExceptionHandler&` at construction,
defaulting to a shared `NullShipmentExceptionHandler` instance returned
from a function-local static:

```cpp
explicit PackageGroup(std::string label,
                       const ShipmentExceptionHandler& handler = null_shipment_exception_handler());
```

`check_weight_limit()` now calls `handler_.on_overweight(...)`
unconditionally, with no `if` anywhere near it. Whether that call does real
work or nothing at all depends entirely on which handler got bound at
construction, never on a branch the method body has to carry. A
`LoggingShipmentExceptionHandler` built for this example's test suite
records every event it is told about, in order, which is what makes it
possible to assert that a real handler actually receives the report while
the default one safely, silently, does not. The reference member is the
detail that makes the contract airtight rather than merely conventional: a
`PackageGroup` cannot be constructed without binding to some
`ShipmentExceptionHandler`, real or null, so there is no state in between
"has a handler" and "has no handler" for a bug to hide in.

## A container contrast: C++20 versus C++23

`SkuCatalog`'s pool and master table are both keyed, ordered maps, and
every operation this example performs on them, insertion, lookup by SKU
code, iteration in sorted key order, works identically whether the
underlying container is a node-based `std::map` or C++23's `std::flat_map`,
a sorted, vector-backed associative container. The difference between the
two is not in what they return. It is in what happens to a reference or an
iterator into the container across a later insertion.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: std::map is a node-based tree. Inserting a new SKU elsewhere in
// the map never invalidates an existing iterator or reference into it,
// because each element lives in its own heap allocation.
std::map<std::string, SkuDescriptor> master_table_;
std::map<std::string, std::shared_ptr<const SkuDescriptor>> flyweights_;
```
```cpp
// C++23: std::flat_map keeps its keys and values in two contiguous
// buffers, which is friendlier to the cache for a pool this small. The
// cost: inserting a new key can reallocate those buffers the way inserting
// into a std::vector can, invalidating every existing iterator and
// reference into the map, not just the one near the insertion point.
std::flat_map<std::string, SkuDescriptor> master_table_;
std::flat_map<std::string, std::shared_ptr<const SkuDescriptor>> flyweights_;
```

`SkuCatalog::get()` never keeps an iterator into either map alive across a
call that might insert into it, and the `std::shared_ptr` it hands back
owns its `SkuDescriptor` on the heap independent of either container's own
storage, so none of this example's call sites feel the trade-off. A caller
that kept an iterator from one `get()` call and dereferenced it after a
second, different `get()` call inserted a new flyweight would feel it
immediately under `std::flat_map` and never notice it under `std::map`,
which is exactly the kind of latent bug a container swap can introduce
silently if the calling code was never written to assume reallocation can
happen. `try_get()`, this example's C++23-only typed lookup, is a separate
refinement: it reports an unregistered SKU as a
`std::expected<std::shared_ptr<const SkuDescriptor>, std::string>` instead
of throwing, the same typed-error idiom Chapter 8 and Chapter 12 built for
their own lookups, guarded the same way behind
`#if __cplusplus >= 202302L` so the mandatory C++20 build path never needs
it.

> **AI sidebar.** Ask an LLM pairing partner to "cache this lookup" or "add
> object pooling" for a value fetched repeatedly by key, and a common result
> reaches straight for a raw pointer or reference returned from a
> hand-rolled cache, with no ownership story for who frees the cached value
> or when. Check the result against the distinction this chapter trained:
> does the cache return a `std::shared_ptr` (or an equivalent owning handle)
> so the caller and the pool can both hold the value safely, or does it
> return a pointer into a container that can reallocate, resize, or get
> destroyed out from under the caller later. The second shape compiles,
> passes a quick manual test, and turns into a use-after-free the first time
> the pool itself grows. A generated "object pool" that never once
> mentions ownership in its returned type is the sign to ask the model
> directly who owns the cached value, and to insist on an answer before
> accepting the code.

## When not to use any of the three

Each of the three patterns in this chapter answers a specific force, and
each one stops paying for itself the moment that force is absent.

Composite earns its interface when a tree can actually get deep: cartons
inside cartons, or a pallet holding both loose items and sub-cartons in the
same shipment. A flat list of items with no nesting on any roadmap does not
need `PackageComponent`; a plain `std::vector<Item>` and a loop that sums
`unit_weight_grams() * quantity()` directly says the same thing with one
interface fewer. Introducing a composite interface ahead of any nesting
adds a virtual call and an owning-pointer collection with nothing
underneath it to justify either cost.

Flyweight pays for itself when the same immutable value is referenced many
times over. A catalog of three SKUs referenced by a dozen
line items saves nothing worth the extra indirection; `SkuDescriptor`
members could live directly on `Item` with no pool at all, and the two
representations would use roughly the same memory. The pattern starts
paying for itself once reference counts climb into the hundreds or
thousands per shared value, which is the point at which a warehouse running
real volume actually sits, and where the memory a naive copy-per-reference
design would spend becomes visible on a profiler rather than theoretical.

Null Object pays for itself when a missing collaborator is a normal,
expected case rather than a programming error. A `PackageGroup` built
without a real exception handler is routine: most cartons are never
overweight, and most code paths that build one do not care about the
handler at all. Reaching for Null Object to paper over a collaborator that
should never legitimately be absent, for instance a missing database
connection a service cannot run without, trades a loud, early failure for a
silent one; that case calls for an exception or a typed error at
construction, not a Null Object quietly doing nothing where a real failure
belongs. The test is whether "absent" is a valid state of the world the
rest of the program should tolerate, or a bug that should be caught as
close to its source as possible.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/17-composite-flyweight` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 18 cases under the `cpp23` preset, 16
under `cpp20`, the difference being the two `std::expected`-specific
`SkuCatalog::try_get()` cases that only compile under C++23. Observed
locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 18   # cpp23
100% tests passed, 0 tests failed out of 16   # cpp20
```

To see all three idioms produce real output rather than just reading an
assertion that says they work, run the example's own demo script, which
builds standalone and runs the result; its `README.md` covers what it does
and how to drive it further:

```sh
examples/17-composite-flyweight/demo.sh cpp23
```

It builds a pallet with a nested carton, reports totals at every level,
builds sixty more line items across three already-registered SKUs,
confirms the flyweight pool stayed at three descriptors, and checks one
carton's weight limit twice, once with the default Null Object and once
with a handler that records what it was told:

```
== Composite: a mixed tree, one interface ==
Carton CTN-A: 14800g, 52 units
Carton CTN-C: 7200g, 8 units
Carton CTN-B: 7200g, 8 units
Pallet PAL-9: 22000g, 60 units

== Flyweight: a few descriptors, many line items ==
Distinct SKUs registered in the catalog: 3
Flyweights materialized so far: 3
After adding 60 more line items across the same 3 SKUs:
Flyweights materialized: 3 (still 3)
Two separate get("WIDGET-100") calls return the same instance: true

== Null Object: a weight-limit check with no branch for "no handler" ==
Carton CTN-A (default, unhandled) checked against a 500g limit:
  (no output above: the default NullShipmentExceptionHandler ran and did nothing)
  Carton CTN-A (logged) is 14300g over its weight limit
```

Notice that Carton CTN-B's total, 7200 grams and 8 units, is exactly Carton
CTN-C's total passed up one level unchanged, and Pallet PAL-9's total,
22000 grams and 60 units, is Carton CTN-A's and Carton CTN-B's totals added
together. That agreement is `PackageGroup::total_weight_grams()` and
`item_count()` recursing uniformly over a mixed tree showing up as an
observable fact at the terminal, not just as a passing assertion. Running
`examples/17-composite-flyweight/demo.sh cpp20` prints the same output
minus the final `std::expected` block, which only compiles under the
`cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design
  Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley):
  the original Composite and Flyweight entries, including the
  intrinsic-versus-extrinsic state distinction this chapter's `SkuDescriptor`
  and `Item` build on. Null Object is not one of the original 23 patterns;
  it was named and popularized afterward as a direct answer to the same
  null-check-at-every-call-site problem this chapter's
  `ShipmentExceptionHandler` solves.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official
  GoogleTest documentation: the testing idiom behind
  `examples/17-composite-flyweight`'s suite, which asserts pointer identity
  for the flyweight pool and a recorded event list for the Null Object
  rather than only checking that the binaries exit zero.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.
