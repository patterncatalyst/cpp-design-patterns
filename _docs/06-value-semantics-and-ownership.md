---
title: "Value semantics, ownership, and smart pointers"
order: 6
part: "cpp-substrate"
description: "Four ownership shapes in a warehouse dispatch system: a value type that follows the Rule of Zero, sole ownership transferred by move with unique_ptr, a resource-owning Rule of Five type shared by shared_ptr, and a parent/child object graph where weak_ptr breaks a reference cycle."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Explain what the Rule of Zero buys a value type, and recognize when a
  type's compiler-generated copy, move, and destructor are already correct.
- Transfer sole ownership of an object between two owners with
  `std::unique_ptr` and `std::move`, and say what "the source is empty
  afterward" actually guarantees.
- Write a resource-owning type that needs the Rule of Five, and share it
  safely between more than one owner with `std::shared_ptr`.
- Break a reference cycle in a parent/child object graph with
  `std::weak_ptr`, and explain why a `shared_ptr` in both directions would
  leak instead.
- Contrast C++20's manual two-step adoption of a raw pointer from a
  C-style factory function with C++23's `std::out_ptr`.
- Recognize when `shared_ptr` is solving an ownership problem a single
  owner already had solved, and say what to write instead.

## Why this matters

Chapter 5 tied a dock bay's lease to the lifetime of a C++ object: acquire
the bay in a constructor, release it in a destructor, and let scope exit
do the cleanup no matter how that scope ends. That discipline answers
*when* something gets released. It does not answer a separate question
this chapter trains: *who* is allowed to release it, and what happens when
more than one part of a program needs to reach the same object, or when an
object's own fields reach back toward something that already owns it.

A parcel moving through a regional warehouse raises exactly that second
question three different ways. A dock holds one parcel at a time and
nobody else may claim it: that is sole ownership, the shape
`std::unique_ptr` exists for. A carrier's rate contract is negotiated once
and quoted against by every route running under it: that is shared
ownership, the shape `std::shared_ptr` exists for. A sub-manifest knows
which parent manifest it split off from, and the parent's own manifest
tree holds that sub-manifest as a child: two objects pointing at each
other, where one direction has to observe without owning or neither object
ever gets freed. `std::weak_ptr` is C++'s answer to that last shape. None
of these is a stylistic choice between three ways to hold a pointer; each
one is the ownership topology the object graph actually has, made explicit
in the type instead of left to a comment telling a future maintainer who
is responsible for calling `delete`.

{% include excalidraw.html name="06-ownership-model" caption="Figure 6.1: Three ownership kinds in one object graph: unique_ptr as a sole owner, shared_ptr as a ref-counted shared owner, and weak_ptr observing without owning to break what would otherwise be a reference cycle." %}

## The domain: dispatching parcels through a regional warehouse

The example behind this chapter lives in `examples/06-value-semantics-ownership`
and stays in the same warehouse Chapter 5 used for its dock bay leases. A
`Parcel` carries a tracking id, a weight, and a label; a `DockSlot` holds at
most one parcel at a time; a `CarrierContract` is a negotiated per-tier rate
table a `ShippingRoute` quotes against; a `ManifestNode` is a sub-manifest in
a shipment's bill-of-lading tree. Four small types, four different ownership
answers, one coherent object graph.

## Value semantics: a Parcel that needs no special members at all

`Parcel` is a plain value type:

```cpp
struct Parcel {
    std::string tracking_id;
    double weight_kg;
    std::string label;
};
```

Nothing here declares a destructor, a copy constructor, a copy assignment
operator, a move constructor, or a move assignment operator, and nothing
needs to. Every member is itself a value type with correct copy and move
behavior already built in: `std::string` owns its own character buffer and
knows how to deep-copy or steal it, and `double` is a value with no
ownership question at all. The compiler generates all five special members
for `Parcel` automatically, and every one of them is already correct. That
is the Rule of Zero: write no special member yourself when every member
you hold already has correct ones, and let the compiler's generated
versions compose theirs. A `Parcel` copy is independent of the original by
construction. Mutate a copy's `weight_kg` after it has been re-weighed at
the hub, and the original parcel's weight is untouched, because copying a
`std::string` and a `double` never shares the thing being copied.

This is the baseline this chapter measures the other three types against.
`Parcel` earns the Rule of Zero because it owns nothing a destructor would
need to clean up. The next three sections introduce types that do, each
for a different, concrete reason.

## Sole ownership: unique_ptr and moving a parcel between docks

A `DockSlot` holds at most one parcel, and that parcel belongs to exactly
one slot at a time:

```cpp
class DockSlot {
public:
    void assign(std::unique_ptr<Parcel> parcel);
    std::unique_ptr<Parcel> release();
    bool occupied() const;
    const Parcel* peek() const;

private:
    std::unique_ptr<Parcel> parcel_;
};
```

`std::unique_ptr<Parcel>` is not just a pointer with automatic cleanup; it
is a type-level promise that nothing else holds a copy of this pointer.
There is no copy constructor to accidentally call, because `unique_ptr`
deletes its copy constructor and copy assignment operator outright. The
only way to give a parcel to a different owner is to move it, and moving a
`unique_ptr` is cheap and exact: the destination's internal pointer
becomes whatever the source's internal pointer was, and the source's
internal pointer becomes `nullptr`. `transfer_parcel()` is the whole
pattern in two lines:

```cpp
void transfer_parcel(DockSlot& from, DockSlot& to) {
    to.assign(from.release());
}
```

`from.release()` hands back the `unique_ptr` it was holding and sets its
own member to `nullptr` in the same call, so `from` is left with nothing,
not a dangling copy of something `to` now also holds. `to.assign()` then
takes ownership of exactly that `unique_ptr` by move. After `transfer_parcel`
returns, `from.occupied()` is `false` and `from.peek()` is `nullptr`; the
parcel itself was never copied, only the single pointer that owns it moved
from one slot's member to the other's. If `to` already held a different
parcel when `assign()` ran, that parcel's `unique_ptr` is overwritten and
its destructor runs immediately, the same automatic cleanup Chapter 5
covered for a lease object going out of scope. No part of this example
ever calls `delete` on a `Parcel*` by hand; the ownership transfer and the
cleanup are both the type system's job, not the programmer's.

## Shared ownership: a contract more than one route depends on

A carrier publishes one rate table and more than one route can run under
the same negotiated contract. `CarrierContract` is the type that owns that
rate table, and it is this chapter's one type that needs the Rule of
Five:

```cpp
class CarrierContract {
public:
    CarrierContract(std::string carrier_name, const Cents* tier_rates_cents, std::size_t tier_count);

    CarrierContract(const CarrierContract& other);
    CarrierContract& operator=(const CarrierContract& other);
    CarrierContract(CarrierContract&& other) noexcept;
    CarrierContract& operator=(CarrierContract&& other) noexcept;
    ~CarrierContract();

    void set_tier_rate(std::size_t tier, Cents rate_cents);
    Cents tier_rate(std::size_t tier) const;

private:
    std::string carrier_name_;
    Cents* tier_rates_ = nullptr;
    std::size_t tier_count_ = 0;
};
```

`tier_rates_` is a raw, heap-allocated `Cents*` rather than a
`std::vector<Cents>`, so this chapter has one real example of a type that
owns a resource the compiler cannot already copy or move correctly on
its own. The compiler-generated copy constructor would copy the pointer
value, not the array it points at, leaving two `CarrierContract` objects
both convinced they own the same heap block; the first one destroyed would
free it, and the second's destructor would free it again. Every one of the
five special members exists to prevent exactly that: the copy constructor
and copy assignment operator allocate a fresh array and copy the rates
into it, so two `CarrierContract` values are independent after a copy the
same way two `Parcel` values are; the move constructor and move assignment
operator steal the source's pointer with `std::exchange` and leave the
source's pointer `nullptr`, so a moved-from contract's destructor finds
nothing to free; and the destructor calls `delete[]` exactly once per
object, on whichever array that object currently owns.

`ShippingRoute` holds a `CarrierContract` through `std::shared_ptr`, not
by value and not through `unique_ptr`:

```cpp
class ShippingRoute {
public:
    ShippingRoute(std::string route_name, std::shared_ptr<CarrierContract> contract, std::size_t tier);
    Cents quote() const;

private:
    std::shared_ptr<CarrierContract> contract_;
    std::size_t tier_;
};
```

Two routes constructed from the same `std::shared_ptr<CarrierContract>`
are not holding two independent copies of the contract the way two
`Parcel` values are independent after a copy. They are holding two
references to the same `CarrierContract` object, and the reference count
behind that object counts both of them. Renegotiate a rate through one
route's contract, `contract->set_tier_rate(1, 475)`, and the other route's
next quote reflects the new rate immediately, because there is only one
`CarrierContract` in memory and both routes point at it. This is the exact
opposite behavior from `Parcel`'s deep-copy independence, and the contrast
is the point: `shared_ptr` is reference semantics, `Parcel`'s plain copy is
value semantics, and a type's ownership model decides which one a caller
gets, not how the caller happens to write the assignment. The
`CarrierContract` object is destroyed exactly once, when the last
`shared_ptr` referencing it goes out of scope or is reset, whichever route
that happens to be.

{% include excalidraw.html name="06-value-vs-reference" caption="Figure 6.2: a Parcel copy staying independent under value semantics against two routes sharing one CarrierContract under reference semantics." %}

## Breaking a cycle: weak_ptr observes without owning

A warehouse's manifest tree adds a third ownership direction: a parent
manifest owns its sub-manifests, and each sub-manifest needs to find its
way back to the parent it split off from.

```cpp
class ManifestNode : public std::enable_shared_from_this<ManifestNode> {
public:
    std::shared_ptr<ManifestNode> add_child(std::string manifest_id);
    std::weak_ptr<ManifestNode> parent() const;

private:
    std::weak_ptr<ManifestNode> parent_;
    std::vector<std::shared_ptr<ManifestNode>> children_;
};
```

`children_` is a vector of `shared_ptr<ManifestNode>`: the parent owns its
children outright, the same shared-ownership shape `CarrierContract` used
above. `parent_` is the reverse edge, and it is a `std::weak_ptr`, not a
`std::shared_ptr`, for a reason that has nothing to do with style. If
`parent_` were also a `shared_ptr`, every child would hold a strong
reference keeping its parent alive, at the same time every parent holds a
strong reference keeping its children alive. Resetting the one external
`shared_ptr<ManifestNode>` a caller holds to the root would not destroy
anything: the root is still reachable from each child it created, and each
child is still reachable from the root's own `children_` vector, so the
pair's reference counts never reach zero and neither object is ever freed.
That is a reference cycle, and it is the classic, well-documented failure
mode of `shared_ptr` used without care.

`std::weak_ptr` does not participate in that count. Calling `child->parent().lock()`
while the root is alive returns a usable `shared_ptr<ManifestNode>`
pointing at it; calling the same `lock()` after the root has been
destroyed returns an empty `shared_ptr`, and `child->parent().expired()`
reports `true`. The warehouse example proves this directly: create a root
manifest, add a child, reset the external `shared_ptr` the caller held to
the root, and the child, still alive because the test itself holds a
`shared_ptr` to it, finds its parent gone. The root's destructor ran the
moment its last strong reference disappeared, exactly as it should, because
the child's reference back to it was never strong enough to keep it alive
on its own.

## Adopting a raw pointer from a C factory: C++20 versus C++23

The warehouse's carrier rates sometimes come from an older, C-style rate
service instead of a negotiated `CarrierContract`: a factory function that
allocates a `CRateTable` on the heap and hands back a raw pointer through
an out-parameter, paired with a function that frees it. Wrapping that raw
pointer in a `std::unique_ptr` immediately, before it can leak or be freed
twice by accident, is the same sole-ownership idea `DockSlot` used above,
applied to a pointer this code did not allocate itself.

```cpp
int legacy_open_rate_table(const char* carrier_code, CRateTable** out_table);
void legacy_close_rate_table(CRateTable* table);
```

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: no std::out_ptr exists yet, so the raw pointer has to be a real,
// named local variable, passed to the legacy function by address, then
// handed to the handle's constructor by hand. The raw pointer is live and
// unmanaged for the lines between the call and the handle construction.
RateTableHandle open_rate_table(const std::string& carrier_code) {
    CRateTable* raw_table = nullptr;
    const int rc = legacy_open_rate_table(carrier_code.c_str(), &raw_table);
    if (rc != 0) {
        return RateTableHandle{};
    }
    return RateTableHandle{raw_table};
}
```
```cpp
// C++23: std::out_ptr binds a temporary raw-pointer slot to the handle
// itself, writes the legacy function's result into it, and adopts the
// pointer into the handle when the call returns. The handle never holds a
// raw pointer and a smart pointer at once.
RateTableHandle open_rate_table(const std::string& carrier_code) {
    RateTableHandle handle;
    const int rc = legacy_open_rate_table(carrier_code.c_str(), std::out_ptr(handle));
    if (rc != 0) {
        return RateTableHandle{};
    }
    return handle;
}
```

Both versions produce the same `RateTableHandle`, a
`std::unique_ptr<CRateTable, RateTableDeleter>` whose deleter calls
`legacy_close_rate_table()`. The difference is how long a bare, unmanaged
`CRateTable*` exists in the function. In the C++20 version, `raw_table` is
a real variable with real scope between the call and the `return`; nothing
stops a line inserted later in that gap from reading it, passing it
somewhere else, or throwing before the handle is ever constructed, and the
table would leak. `std::out_ptr(handle)` in the C++23 version produces a
temporary pointer that writes directly into `handle`'s own storage when
the call that received it returns, so there is no window at all where the
pointer exists outside a `unique_ptr`'s control. `std::inout_ptr` is the
companion for a C API that also needs to read the handle's current pointer
before overwriting it, for instance a resize-in-place function; this
example's factory only ever allocates fresh, so `out_ptr` is the one it
needs. `examples/06-value-semantics-ownership/src/rate_table_adapter.cpp`
guards the C++23 branch behind `#if __cplusplus >= 202302L`, so the
mandatory C++20 build path never requires `std::out_ptr`.

> **AI sidebar.** Ask an LLM pairing partner to "add a smart pointer" around a
> raw pointer a function returns and a common result reaches for
> `std::shared_ptr` by default, because `shared_ptr` is the smart pointer
> that appears most often in generated snippets and tolerates being copied
> carelessly without an immediate compile error. Check the result against
> the ownership the object graph actually has, the same way this chapter
> trained: if nothing else needs to reference the object, a `unique_ptr`
> says that in the type and costs nothing extra at runtime a `shared_ptr`
> would not also cost; if the object sits in a parent/child graph, check
> whether a generated back-reference from child to parent is a `shared_ptr`
> pointing the wrong way, because that specific mistake is exactly the
> cycle this chapter's `ManifestNode` exists to show how to avoid. A model
> rarely volunteers `weak_ptr` on its own, since nothing about a single
> generated snippet forces it to reason about a cycle spanning two classes
> it is not looking at together.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/06-value-semantics-ownership` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 22 cases under both the `cpp23` and
`cpp20` presets, because `open_rate_table()` compiles to a different
implementation under each standard but exposes the identical signature
either way, so no test is compiled out between them. Observed locally on
Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 22   # cpp23
100% tests passed, 0 tests failed out of 22   # cpp20
```

To see all four ownership shapes play out rather than just reading an
assertion that says they hold, run the example's own demo script:

```sh
examples/06-value-semantics-ownership/demo.sh cpp23
```

It copies and mutates a `Parcel`, transfers a parcel between two
`DockSlot`s by move, renegotiates a `CarrierContract` shared by two
routes, resets a manifest's root and shows its child's `weak_ptr` expire,
and opens a legacy rate table through `std::out_ptr`:

```
== Value semantics: a Parcel copy is independent ==
original: TRK-1001, 4.2 kg, "Fragile - this side up"
copy:     TRK-1001, 9.9 kg, "Re-weighed at the hub"

== Sole ownership: unique_ptr transfers a Parcel between docks ==
before transfer: inbound occupied=yes, outbound occupied=no
after transfer:  inbound occupied=no, outbound occupied=yes (TRK-2002)

== Shared ownership: two routes quote against one CarrierContract ==
north-corridor quote (before renegotiation): $4.00
south-corridor quote (before renegotiation): $4.00
north-corridor quote (after renegotiation): $4.75
south-corridor quote (after renegotiation): $4.75

== Breaking a cycle: weak_ptr observes a manifest's parent ==
child's parent while root is alive: M-1000
child's parent after root is reset: expired

== C++23 refinement: adopting a legacy C factory's pointer ==
cardinal per-kg rate: 310 cents (handle releases it automatically)
unknown-carrier: rejected, no handle allocated
```

Running `examples/06-value-semantics-ownership/demo.sh cpp20` prints
identical output: `open_rate_table()`'s observable behavior does not
change between standards, only the mechanism it uses internally to adopt
the legacy pointer.

## When not to reach for shared_ptr

Every ownership type in this chapter earns its cost only when the object
graph actually has the shape that type answers. The most common mistake
going the other direction is reaching for `shared_ptr` by default, the
same reflex the AI sidebar above named, when a single, clear owner already
exists.

A `DockSlot` that held its `Parcel` through `shared_ptr` instead of
`unique_ptr` would compile and run identically for every case this
chapter's tests cover, because nothing stops a `shared_ptr` from behaving
like a sole owner when nothing else ever copies it. But that version would
also silently permit a second piece of code to retain a copy of the
pointer past the point the warehouse considers the parcel transferred,
with no compiler error marking the mistake, where `unique_ptr`'s deleted
copy constructor would have refused to compile the moment a second owner
tried to hold the same parcel. `shared_ptr` costs more than `unique_ptr`
too: an atomic reference count updated on every copy and every
destruction, and in the common `make_shared` form, a control block
allocated alongside the object. Reach for `shared_ptr` only when a second
owner is real, not hypothetical, the same forces-over-reflex habit Part 1
trained for choosing a pattern at all.

The same caution runs the other way for `Parcel`'s value semantics. A
`Parcel` that carried a large, expensive-to-copy buffer, a full scanned
image of its shipping label rather than a short string, would make every
copy in this chapter's demo an expensive deep copy of that image, repeated
on every function call that took a `Parcel` by value instead of by
reference. Value semantics are the right default because a copy that is
independent is easier to reason about than a reference that
might be shared, but a type whose members are expensive to duplicate
should pass by `const&` where no copy is needed, and should consider
`std::shared_ptr<const Image>` for the one field that is big and rarely
mutated, rather than deep-copying the whole `Parcel` on every pass. The
Rule of Zero answers whether a type needs special members to write by
hand; it does not answer whether copying that type by value, as often as
the surrounding code happens to do it, is still cheap enough to be the
default.

## For deeper coverage

- Bjarne Stroustrup, [*The C++ Programming Language*](https://www.informit.com/store/c-plus-plus-programming-language-9780321563842), 4th Edition
  (Addison-Wesley): the resource management chapter this book's Rule of
  Zero and Rule of Five both draw from, including the vocabulary of value
  versus reference semantics this chapter contrasts directly.
- Scott Meyers, [*Effective Modern C++*](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/) (O'Reilly): Items 18-21 cover
  `unique_ptr`, `shared_ptr`, and `weak_ptr` in the depth this chapter
  assumes, including the reference-cycle failure mode `ManifestNode`
  exists to avoid.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind
  `examples/06-value-semantics-ownership`'s suite, which asserts observable
  ownership behavior (an emptied source after a move, an expired
  `weak_ptr`, a renegotiated rate seen by every sharing route) rather than
  just checking that the binary exits zero.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each citation
  there is a pointer to the full book, not a substitute for reading it.
