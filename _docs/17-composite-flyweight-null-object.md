---
title: "Composite, Flyweight, and Null Object"
order: 17
part: "composing-adapting-structure"
description: "Three idiomatic ways to keep a recursive structure simple, taught against a palletizing domain: a Composite interface that treats one item and an entire pallet identically, a Flyweight pool that shares a handful of SKU descriptors across thousands of line items, and a Null Object that lets a weight-limit check skip every null branch."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Treat a single object and an arbitrarily deep tree of objects through one interface, with no branch telling leaf from group.
- Build a Composite whose leaves and groups recurse correctly, including a group nested inside another group.
- Share immutable data across many references with a Flyweight pool, and prove the sharing with an identity check.
- Replace a null check at every call site with a Null Object bound at construction.
- Choose between `std::map` and `std::flat_map`, new in C++23, for a small, rarely mutated pool.
- Recognize when a plain class, a `std::vector` or a `nullptr` check already solves the problem.

## Why this matters

A pallet is a tree. It holds cartons, a carton can hold more cartons before it holds an item, and every level asks the same questions: how much does this weigh, and how many units does it hold. Separate functions for items, cartons and pallets duplicate the work, and the first time a warehouse consolidates two partial shipments into one box, the carton function has to learn to call itself.

Once the pallet ships, one SKU, a battery pack or a gasket, can appear on hundreds of line items. Each needs the same weight, dimensions and hazmat class; only quantity and destination differ. Copying the description into every line item is correct and wasteful.

A carton's weight-limit check also needs somewhere to report a violation, and usually nothing is wired up to receive it. Guarding every call site with "if there is a handler" works until a new call site forgets.

Three different forces, one palletizing domain.

{% include excalidraw.html name="17-composite-flyweight" caption="Figure 17.1: a composite tree (pallet, cartons, items) sharing one interface on the left; a flyweight pool of SKU descriptors referenced by many line items on the right." %}

Each item references a SKU descriptor:

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

Nothing in `SkuDescriptor` varies per line item. A `WIDGET-100` weighs 250 grams on every pallet, which is what makes it safe to share.

## Pattern: Composite

### The problem

Client code needs the weight and unit count of an item, a carton, a carton of cartons or a whole pallet, without knowing which one it holds or how deep the nesting goes.

### The solution

**A Composite gives leaves and groups of components one interface, so a client treats a single object and a tree of objects the same way.**

### How C++ models it

Both leaf and group implement `PackageComponent`:

```cpp
class PackageComponent {
public:
    virtual ~PackageComponent() = default;
    virtual int total_weight_grams() const = 0;
    virtual int item_count() const = 0;
    virtual const std::string& label() const = 0;
};
```

`Item` is the leaf. It holds a shared `SkuDescriptor` plus the two facts that are its own:

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

`PackageGroup` is the composite. Cartons and pallets are the same class, because to `PackageComponent` both are a collection of children. `add()` accepts any component, so a group holds items, other groups, or both:

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

The recursion never asks what kind of child it's looking at:

```cpp
int PackageGroup::total_weight_grams() const {
    return std::accumulate(children_.begin(), children_.end(), 0,
                            [](int sum, const std::shared_ptr<PackageComponent>& child) {
                                return sum + child->total_weight_grams();
                            });
}
```

`item_count()` is the same `std::accumulate` over `item_count()`. A carton nested two levels deep recurses into its own children, and the outer carton never knows the nesting is there.

### Tree or flat vector

A flat `std::vector<Item>` answers "how much does this pallet weigh" as well as a tree. It can't answer a sub-tree's question: the weight of one carton, moving a whole carton between pallets as a unit, or consolidating two partial cartons without re-flattening their items. When the domain needs answers at intermediate levels, the tree pays.

## Pattern: Flyweight

### The problem

Hundreds of line items reference a few SKUs. Storing the full descriptor in each line item makes the copies, not the logic, the memory problem.

### The solution

**A Flyweight shares one immutable instance of the state that never changes (intrinsic state) among many objects, which keep only their own varying state (extrinsic state).**

### How C++ models it

`SkuCatalog` is both factory and pool. It keeps a master table of every registered SKU and a pool of flyweights materialized so far:

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

A warehouse catalog can list thousands of SKUs that never touch a given pallet, so `get()` materializes a descriptor only on first request. Every later call returns the same `std::shared_ptr`. `pool_size()` counts distinct SKUs shared, not the items that reference them: sixty more line items across the same three SKUs leave it at three.

### Identity, not equality

Two `get("WIDGET-100")` calls return pointers that compare equal with `.get()`, so the second call reused the first allocation. A pool that returns `SkuDescriptor` by value compiles, passes a field-equality check, and still allocates one descriptor per `Item`. Check identity to catch it.

### `std::map` or `std::flat_map`

Every operation `SkuCatalog` performs, insertion, lookup by code and sorted iteration, works the same on both containers. They differ in what an insertion does to existing iterators and references.

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

| | `std::map` | `std::flat_map` |
| --- | --- | --- |
| Storage | One heap node per element | Two contiguous sorted buffers |
| Iterators after an insert | Stay valid | May all be invalidated |
| Fits | Frequent inserts, long-lived iterators | Small, read-mostly tables |

`get()` never holds an iterator across an insertion, and the returned `std::shared_ptr` owns its descriptor apart from either container's storage, so the swap is safe here. A caller that kept an iterator from one `get()` and used it after another `get()` inserted would break under `std::flat_map` only.

`try_get()` reports an unregistered SKU as a `std::expected<std::shared_ptr<const SkuDescriptor>, std::string>` instead of throwing, behind `#if __cplusplus >= 202302L`.

## Pattern: Null Object

### The problem

`PackageGroup` can report an overweight condition, but usually nothing receives the report. The guarded call looks like this:

```cpp
if (handler_ != nullptr && total > grams_limit) {
    handler_->on_overweight(label_, total - grams_limit);
}
```

It's correct, and it's a check a reviewer must re-verify at every call site that touches `handler_`, because nothing in the type system stops the next method from forgetting it.

### The solution

**A Null Object is a do-nothing implementation of a collaborator's interface that stands in for "no collaborator", so callers call it unconditionally.**

### How C++ models it

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

`PackageGroup` binds a `const ShipmentExceptionHandler&` at construction, defaulting to a shared instance returned from a function-local static:

```cpp
explicit PackageGroup(std::string label,
                       const ShipmentExceptionHandler& handler = null_shipment_exception_handler());
```

`check_weight_limit()` calls `handler_.on_overweight(...)` with no `if`. Which handler was bound decides whether the call does work. Because the member is a reference, a `PackageGroup` can't exist without some handler, real or null, so there's no third state for a bug to hide in. `LoggingShipmentExceptionHandler` records every event in order, for callers that want the report.

## When not to use it

- **Composite.** A flat list of items that never nests doesn't need `PackageComponent`. A `std::vector<Item>` and a loop over unit weight times quantity says the same thing without a virtual call and an owning-pointer collection.
- **Flyweight.** Three SKUs referenced by a dozen line items save nothing worth the indirection; put the `SkuDescriptor` fields on `Item`. The pool pays once references per value climb into the hundreds or thousands and the copies show on a profiler.
- **Null Object.** Use it when an absent collaborator is a normal state, as an unwired exception handler is. When the collaborator must exist, such as a database connection a service can't run without, a Null Object turns a loud early failure into a silent one; throw or return a typed error at construction instead.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a flyweight pool.** "Cache this lookup" leaves open who owns the cached value and whether callers get a shared instance or a copy. State both:

> Write `SkuCatalog::get(std::string_view sku_code)` returning `std::shared_ptr<const SkuDescriptor>`. Materialize a descriptor from the master table on first request, store it in `flyweights_`, and return the same pointer on every later call. Throw `std::out_of_range` for an unregistered code. Never return a raw pointer, reference or iterator into either map, and don't materialize every registered SKU up front.

Run `ctest --preset cpp23 -R SkuCatalog`, and check that two `get()` calls for one code return the same `.get()` address.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/17-composite-flyweight`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 18 cases pass under `cpp23` and 16 under `cpp20`; the two `try_get()` cases compile only under C++23. The demo builds standalone; its `README.md` covers driving it further:

```sh
examples/17-composite-flyweight/demo.sh cpp23
```

It builds a pallet with a nested carton, adds sixty line items across three registered SKUs, and checks one carton's weight limit with the default handler and then a logging one:

```
== Composite: a mixed tree, one interface ==
Carton CTN-A: 14800g, 52 units
Carton CTN-C: 7200g, 8 units
Carton CTN-B: 7200g, 8 units
Pallet PAL-9: 22000g, 60 units
(Pallet's totals are Carton CTN-A's and Carton CTN-B's totals added together,
 and Carton CTN-B's total is Carton CTN-C's total, recursed through two levels.)

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

== C++23 refinement: typed error for an unregistered SKU ==
Rejected lookup: unregistered SKU code: CRATE-999
```

CTN-B passes CTN-C's 7,200 g up unchanged, and PAL-9's 22,000 g is CTN-A plus CTN-B. Under `cpp20` the output stops before the `std::expected` block.

- **Stop pooling.** In a scratch copy, delete the two `flyweights_.insert_or_assign(key, flyweight);` lines in `src/sku_catalog.cpp`. Four of the 18 cases fail: the identity and pool-size checks.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Composite and Flyweight entries, including the intrinsic-versus-extrinsic state distinction behind `SkuDescriptor` and `Item`. Null Object is not one of the original 23 patterns; it was named afterward as an answer to the null-check-at-every-call-site problem.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/17-composite-flyweight`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
