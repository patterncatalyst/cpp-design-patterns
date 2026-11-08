---
title: "Value semantics, ownership, and smart pointers"
order: 6
part: "cpp-substrate"
description: "Four ownership shapes in a warehouse dispatch system: a value type that follows the Rule of Zero, sole ownership transferred by move with unique_ptr, a resource-owning Rule of Five type shared by shared_ptr, and a parent/child object graph where weak_ptr breaks a reference cycle."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Say what the Rule of Zero buys a value type, and recognize when the compiler-generated special members are already correct.
- Transfer sole ownership between two owners with `std::unique_ptr` and `std::move`, and say what "the source is empty afterward" guarantees.
- Write a resource-owning type that needs the Rule of Five, and share it between owners with `std::shared_ptr`.
- Break a parent/child reference cycle with `std::weak_ptr`, and say why `shared_ptr` in both directions leaks.
- Contrast C++20's two-step adoption of a raw pointer from a C factory with C++23's `std::out_ptr`.
- Recognize a `shared_ptr` that's standing in for a single owner.

## Why this matters

Chapter 5 answered *when* a resource is released: at scope exit, however the scope ends. The next question is *who* may release it, when several parts of a program reach the same object, or when an object points back at its owner.

A parcel moving through a regional warehouse raises that question in three ways:

| Relationship | Shape | Type |
| --- | --- | --- |
| A dock holds one parcel, and nobody else may claim it | sole ownership | `std::unique_ptr` |
| Every route under a carrier's rate contract quotes against the same contract | shared ownership | `std::shared_ptr` |
| A sub-manifest knows the parent manifest that owns it | an observer that doesn't own | `std::weak_ptr` |

None of these is style. Each is the ownership the object graph has, written into the type instead of a comment saying who calls `delete`.

{% include excalidraw.html name="06-ownership-model" caption="Figure 6.1: Three ownership kinds in one object graph: unique_ptr as a sole owner, shared_ptr as a ref-counted shared owner, and weak_ptr observing without owning to break what would otherwise be a reference cycle." %}

## The domain: dispatching parcels through a regional warehouse

The code is in `examples/06-value-semantics-ownership`, in the same warehouse as Chapter 5:

| Type | Role | Ownership |
| --- | --- | --- |
| `Parcel` | tracking id, weight and label | value |
| `DockSlot` | holds at most one parcel | sole owner |
| `CarrierContract` | a negotiated per-tier rate table that a `ShippingRoute` quotes against | shared |
| `ManifestNode` | a sub-manifest in a shipment's bill-of-lading tree | owns children, observes parent |

## Value semantics: a Parcel with no special members

```cpp
struct Parcel {
    std::string tracking_id;
    double weight_kg;
    std::string label;
};
```

`Parcel` declares none of the five special members and needs none. `std::string` already copies and moves its own buffer correctly, and `double` has no ownership question. The compiler generates all five, and each is correct. That's the Rule of Zero: write no special member when every member already has correct ones.

A copy is independent by construction. Re-weigh a copy at the hub and the original's `weight_kg` is unchanged. `Parcel` earns the Rule of Zero because it owns nothing a destructor must clean up; the next three types do.

## Sole ownership: unique_ptr moves a parcel between docks

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

`std::unique_ptr<Parcel>` promises in the type that nothing else holds this pointer. Its copy operations are deleted, so the only way to hand a parcel on is a move: the destination takes the pointer and the source becomes `nullptr`.

```cpp
void transfer_parcel(DockSlot& from, DockSlot& to) {
    to.assign(from.release());
}
```

Afterward `from.occupied()` is `false` and `from.peek()` is `nullptr`. The parcel was never copied; only the pointer that owns it moved. If `to` already held a parcel, `assign()` overwrites its `unique_ptr` and that parcel is destroyed on the spot. No code calls `delete` by hand.

## Shared ownership: a contract several routes depend on

One carrier rate table serves several routes. `CarrierContract` owns that table and needs the Rule of Five:

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

`tier_rates_` is a raw heap array instead of a `std::vector<Cents>`, so there's one type whose resource the compiler can't copy or move correctly. A generated copy would copy the pointer, two contracts would own one block, and the second destructor would free it again. Each special member prevents that:

| Member | Does |
| --- | --- |
| copy constructor, copy assignment | allocate a fresh array and copy the rates, so copies are independent |
| move constructor, move assignment | take the source's pointer with `std::exchange` and leave it `nullptr` |
| destructor | `delete[]` once, on the array this object owns |

`ShippingRoute` holds the contract through `std::shared_ptr`:

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

Two routes built from the same `shared_ptr<CarrierContract>` refer to one object, and its reference count includes both. Renegotiate through one, `contract->set_tier_rate(1, 475)`, and the other's next quote sees the new rate. That's reference semantics, the opposite of `Parcel`'s independent copies, and the type's ownership model decides which one the caller gets. The contract is destroyed once, when the last `shared_ptr` to it goes away.

{% include excalidraw.html name="06-value-vs-reference" caption="Figure 6.2: a Parcel copy staying independent under value semantics against two routes sharing one CarrierContract under reference semantics." %}

## Breaking a cycle: weak_ptr observes without owning

A parent manifest owns its sub-manifests, and each sub-manifest needs a way back to its parent:

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

The parent owns its children through `children_`. If `parent_` were also a `shared_ptr`, each child would keep its parent alive while the parent keeps the child alive. Resetting the caller's only `shared_ptr` to the root would destroy nothing: the counts never reach zero. That's a reference cycle, the classic `shared_ptr` leak.

`std::weak_ptr` doesn't count. While the root is alive, `child->parent().lock()` returns a usable `shared_ptr`. Once the root's last strong reference is gone, the root is destroyed, `lock()` returns an empty `shared_ptr`, and `child->parent().expired()` is `true`, even while the child itself is still held.

## Adopting a raw pointer from a C factory: C++20 versus C++23

Some carrier rates come from an older C-style service: a factory that allocates a `CRateTable` and returns it through an out-parameter, paired with a function that frees it.

```cpp
int legacy_open_rate_table(const char* carrier_code, CRateTable** out_table);
void legacy_close_rate_table(CRateTable* table);
```

Wrap the pointer in a `std::unique_ptr` straight away, the same sole ownership `DockSlot` uses, applied to a pointer this code didn't allocate.

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

`RateTableHandle` is a `std::unique_ptr<CRateTable, RateTableDeleter>` whose deleter calls `legacy_close_rate_table()`. The difference is how long an unmanaged `CRateTable*` exists. In C++20, `raw_table` lives from the call to the `return`, and a line added later in that gap could copy it, pass it on or throw, and the table leaks. `std::out_ptr`, new in C++23, writes the result into `handle` when the call returns, so the pointer is never outside a `unique_ptr`. Its companion `std::inout_ptr` is for a C API that also reads the current pointer before replacing it, such as a resize in place; this factory only allocates. `src/rate_table_adapter.cpp` guards the C++23 branch with `#if __cplusplus >= 202302L`.

<blockquote class="ai-sidebar" markdown="1">
**Specifying ownership.** "Add a smart pointer" doesn't say how many owners the object has or which edges in a graph must not own. State those:

> In `examples/06-value-semantics-ownership`, add `ManifestNode::sibling_manifests()` returning the other children of this node's parent. Keep `parent_` a `std::weak_ptr<ManifestNode>` and `lock()` it inside the function; don't add any `std::shared_ptr` member that points from a child toward its parent. Return the siblings as `std::vector<std::shared_ptr<ManifestNode>>`. Leave `DockSlot` on `std::unique_ptr<Parcel>`; don't change it to `shared_ptr`.

Check with `ctest --preset cpp23 -R ManifestTree`: the expiry cases must still pass after the root is reset.
</blockquote>

## When not to reach for shared_ptr

A `DockSlot` holding `shared_ptr<Parcel>` would compile and pass every existing case, because a `shared_ptr` nobody copies acts like a sole owner. It would also let another piece of code keep the parcel after the warehouse considers it transferred, with no compiler error, where `unique_ptr`'s deleted copy would refuse to build.

| | `std::unique_ptr` | `std::shared_ptr` |
| --- | --- | --- |
| A second owner | compile error | silently allowed |
| Cost per copy and destruction | none | an atomic reference-count update |
| Allocation | the object | the object plus a control block (one allocation with `make_shared`) |
| Use when | one owner, which is the common case | a second owner is real, not hypothetical |

Value semantics have a limit too. A `Parcel` carrying a full scanned image of its label would deep-copy that image every time a function took it by value. Pass it by `const&` where no copy is needed, and consider `std::shared_ptr<const Image>` for the one large, rarely changed field. The Rule of Zero says whether you must write special members; it doesn't say copying is still cheap.

## Build, run, observe

From the repository root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 22 cases pass under both presets: `open_rate_table()` compiles differently under each standard but keeps one signature, so no case is compiled out. Then run the demo:

```sh
examples/06-value-semantics-ownership/demo.sh cpp23
```

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

Under `cpp20` the output is identical; only the mechanism inside `open_rate_table()` changes.

- **Own the parent.** In a scratch copy, make `parent_` a `std::shared_ptr<ManifestNode>` and set it from `shared_from_this()`. Two cases fail: after the root is reset, a child and a grandchild still reach a live parent, because the cycle keeps it alive.

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
  `examples/06-value-semantics-ownership`, which checks observable
  ownership behavior (an emptied source after a move, an expired
  `weak_ptr`, a renegotiated rate seen by every sharing route) rather
  than just an exit code.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each citation
  there is a pointer to the full book, not a substitute for reading it.
