---
title: "Shortcut patterns: dispatch tables and maps"
order: 25
part: "patterns-in-the-wild"
description: "Three lightweight alternatives to a full Strategy, State, or Command class hierarchy, taught against a shipment-tracking ingestion service: an enum-indexed function-pointer table (the classic jump table), a std::unordered_map dispatch map for an open, runtime-extensible set, and a constexpr std::array table checked complete at compile time."
duration: "25 min"
---

## Learning objectives

By the end of this chapter you can:

- Build an enum-indexed function-pointer table and name the positional convention its correctness depends on.
- Build a `std::unordered_map` dispatch map for an open key set, and register a handler after startup.
- Build a `constexpr std::array` table with enumerator-derived indices and prove it complete with a `static_assert`.
- Say what `std::to_underlying` adds over a bare `static_cast`, and when `std::flat_map` suits a write-rarely, read-often table.
- Tell when a key set has outgrown a table and needs Strategy, a state machine or Command.
- Tell when a short `if`/`else if` chain is the better choice.

## Why this matters

A shipment-tracking service sits behind several carrier webhooks, and every inbound message carries a type tag: created, picked up, moved to a hub, loaded for the final leg, delivered, or bounced as undeliverable. Something has to read the tag and run the one function that handles it. An `if (type == CarrierMessageType::Created) { ... } else if ...` chain grows with every message kind, and the function ends up doing six unrelated things.

Strategy, State and Command earn their weight when each branch holds real behavior: a `PaidState` with invariants to protect, a `MoveStockCommand` that remembers its own inverse. A dispatcher that calls one short function per tag is a smaller problem. A `CreatedMessageHandler` class with a constructor, a virtual `handle()` and a factory, wrapped around three lines that never vary, is over-engineering. A table or a map is the right size.

{% include excalidraw.html name="25-dispatch-tables" caption="Figure 25.1: a carrier webhook feed routes through one of three dispatch mechanisms (jump table, dispatch map, constexpr table), closed-set forms on the left, the open, runtime-extensible map on the right, all three funneling into shared handler functions and a TrackingLedger." %}

Messages arrive in two shapes. `CarrierMessage` carries a closed enum compiled into the service:

```cpp
enum class CarrierMessageType {
    Created,
    PickedUp,
    InTransit,
    OutForDelivery,
    Delivered,
    DeliveryException,
};

struct CarrierMessage {
    CarrierMessageType type;
    std::string tracking_id;
    std::string detail;
};
```

`CarrierWireMessage` carries a `wire_tag` string, whatever a third-party carrier sends, which the service can't enumerate ahead of time:

```cpp
struct CarrierWireMessage {
    std::string wire_tag;
    std::string tracking_id;
    std::string detail;
};
```

All three forms call the same six handlers, `handle_created` through `handle_delivery_exception`, each writing one note to a `TrackingSink`, the interface `TrackingLedger` implements. Changing the routing mechanism never changes the behavior behind a route.

## Pattern: Jump table

### The problem

The six message types are fixed and known when the code is written. Each dispatch should cost as little as possible: no branch chain, no hash, no virtual call.

### The solution

A **jump table** is an array of function pointers indexed by the enum value, so dispatch is one bounds check and one indexed call.

### How C++ models it

```cpp
using MessageHandler = void (*)(const CarrierMessage&, TrackingSink&);

MessageHandler g_jump_table[kCarrierMessageTypeCount] = {
    &handle_created,
    &handle_picked_up,
    &handle_in_transit,
    &handle_out_for_delivery,
    &handle_delivered,
    &handle_delivery_exception,
};

void dispatch_via_jump_table(const CarrierMessage& message, TrackingSink& sink) {
    const std::size_t index = message_type_index(message.type);
    if (index >= kCarrierMessageTypeCount) {
        handle_unrecognized_type(message, sink);
        return;
    }
    g_jump_table[index](message, sink);
}
```

The bounds check guards a value deserialized from an untrusted payload: it can carry any integer, and indexing past the array is undefined behavior.

### Rows must follow the enum

`message_type_index()` is the enumerator's position used as a subscript, so the rows of `g_jump_table` must stay in declaration order. Insert `ReturnedToSender` in the middle of the enum without inserting its row at the same position, and every later handler answers for the wrong type. The compiler says nothing. Speed and minimal code are bought with a convention people maintain by hand.

### `std::to_underlying`

`message_type_index()` is where the two standards differ. It's guarded by `#if __cplusplus >= 202302L` in `src/carrier_message.hpp`, so both closed-set forms call the same function under either standard.

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: a bare static_cast converts the enum straight to std::size_t. It
// compiles, and it is correct, but it says nothing about *how* the
// conversion happens, only that it does.
inline constexpr std::size_t message_type_index(CarrierMessageType type) {
    return static_cast<std::size_t>(type);
}
```
```cpp
// C++23: std::to_underlying names the intermediate step explicitly,
// converting to the enum's declared underlying type first, before the
// outer static_cast widens that to std::size_t. The result is identical;
// the code now says what it means instead of relying on a reader to know
// that casting an enum class straight to size_t is shorthand for exactly
// this.
inline constexpr std::size_t message_type_index(CarrierMessageType type) {
    return static_cast<std::size_t>(std::to_underlying(type));
}
```

The result is identical for `CarrierMessageType`, whose underlying type is `int`. `std::to_underlying`, new in C++23, states the step to the enum's underlying type, which matters most for an enum that declares a narrower one.

## Pattern: Dispatch map

### The problem

A carrier integrated next quarter will send wire tags nobody has written an enumerator for. Adding a handler shouldn't require a recompile or an edit to the dispatcher.

### The solution

A **dispatch map** keys handlers by a runtime value in an associative container, so handlers can be added while the program runs.

### How C++ models it

{% raw %}
```cpp
class MessageDispatchMap {
public:
    using Handler = std::function<void(const CarrierWireMessage&, TrackingSink&)>;

    MessageDispatchMap() {
        handlers_ = {
            {"CREATED", adapt(CarrierMessageType::Created, &handle_created)},
            {"PICKED_UP", adapt(CarrierMessageType::PickedUp, &handle_picked_up)},
            // ... one more row per built-in wire tag
        };
    }

    void register_handler(std::string wire_tag, Handler handler) {
        handlers_[std::move(wire_tag)] = std::move(handler);
    }

    void dispatch(const CarrierWireMessage& message, TrackingSink& sink) const {
        const auto it = handlers_.find(message.wire_tag);
        if (it == handlers_.end()) {
            handle_unrecognized_wire_tag(message, sink);
            return;
        }
        it->second(message, sink);
    }

private:
    std::unordered_map<std::string, Handler> handlers_;
};
```
{% endraw %}

`register_handler()` is why to choose a map over an array. The demo registers `"CUSTOMS_HOLD"` after construction, and the next `dispatch()` routes to it with no restart.

The self-registering registry from [expression templates and generic idioms]({{ '/docs/23-expression-templates-and-generic-idioms/' | relative_url }}) also keeps a set open, through static initialization in each type's own translation unit:

| Who registers | Use |
| --- | --- |
| The codebase's own types | Self-registration saves a step |
| An operator onboarding a carrier at run time | An explicit `register_handler()` call at a visible site |

### Unknown keys need an answer

An unseeded, unregistered tag falls back to `handle_unrecognized_wire_tag()`, which records the raw tag for whoever reviews the ledger. A closed enum guarantees every constructible value was planned for, out-of-range casts aside; a string key guarantees nothing. Give every dispatch map an unknown-key branch from the start.

### `std::flat_map`

The map is written a handful of times at startup or onboarding and read on every message. `std::unordered_map` allocates a node per entry, and each lookup pays a hash and a pointer chase. `std::flat_map`, new in C++23, keeps keys and values in two sorted contiguous vectors: an O(log n) binary search over memory the CPU can prefetch.

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: std::flat_map does not exist yet. A dispatch map that wants
// contiguous storage has to use std::map (a sorted tree, still node-based)
// or accept std::unordered_map's hash-table layout.
std::unordered_map<std::string, Handler> handlers_;
```
```cpp
// C++23: std::flat_map gives the same ordered-map interface over two
// contiguous vectors, better cache behavior for a table that is read far
// more often than it is written.
std::flat_map<std::string, Handler> handlers_;
```

`FlatMessageDispatchMap`, behind `#if __cplusplus >= 202302L` in `src/dispatch_map.hpp` and `.cpp`, changes only that member's type. The C++20 build never includes `<flat_map>`.

## Pattern: Constexpr dispatch table

### The problem

The jump table is fast, but its rows can drift out of sync with the enum, and a missing row surfaces as a wrong handler or a null call in production.

### The solution

A **constexpr dispatch table** fills each slot at an index computed from its enumerator inside a `constexpr` function, so a `static_assert` can check every slot before the program exists.

### How C++ models it

```cpp
constexpr std::array<MessageHandler, kCarrierMessageTypeCount> make_dispatch_table() {
    std::array<MessageHandler, kCarrierMessageTypeCount> table{};
    table[message_type_index(CarrierMessageType::Created)] = &handle_created;
    table[message_type_index(CarrierMessageType::PickedUp)] = &handle_picked_up;
    table[message_type_index(CarrierMessageType::InTransit)] = &handle_in_transit;
    table[message_type_index(CarrierMessageType::OutForDelivery)] = &handle_out_for_delivery;
    table[message_type_index(CarrierMessageType::Delivered)] = &handle_delivered;
    table[message_type_index(CarrierMessageType::DeliveryException)] = &handle_delivery_exception;
    return table;
}

inline constexpr auto kDispatchTable = make_dispatch_table();

static_assert(std::ranges::none_of(kDispatchTable, [](MessageHandler handler) { return handler == nullptr; }),
              "every CarrierMessageType must have a handler entry in kDispatchTable");
```

Two things changed from the jump table:

- **Order doesn't matter.** Each assignment names its enumerator, so reordering the lines changes nothing.
- **A missing row fails the build.** `table{}` starts every slot at `nullptr`, so a forgotten line leaves a null entry, and the `static_assert` rejects it with a message naming the problem.

Dispatch reads like the jump table's:

```cpp
void dispatch_via_constexpr_table(const CarrierMessage& message, TrackingSink& sink) {
    const std::size_t index = message_type_index(message.type);
    if (index >= kCarrierMessageTypeCount) {
        handle_unrecognized_type(message, sink);
        return;
    }
    kDispatchTable[index](message, sink);
}
```

The bounds check stays: the `static_assert` covers every declared enumerator, not a garbled integer from outside the enum's range.

> **🤖 Explore with your agent**
>
> "In `examples/25-dispatch-tables/src/carrier_message.hpp`, add a `ReturnedToSender` enumerator at the end of `CarrierMessageType` and raise `kCarrierMessageTypeCount` to 7. Declare and define `handle_returned_to_sender` in `message_handlers.hpp` and `.cpp`. In `dispatch_table_constexpr.hpp`, add its row to `make_dispatch_table()` as `table[message_type_index(CarrierMessageType::ReturnedToSender)] = ...`, keep the `std::ranges::none_of` `static_assert`, and use no `std::map` or `std::function` for the closed set. Give `MessageDispatchMap` a `"RETURNED_TO_SENDER"` row, and append the handler as the last row of `g_jump_table` so its rows stay in enum declaration order."
>
> Check: the build succeeds, `ctest --preset cpp23 -R 'JumpTableTest|ConstexprDispatchTableTest|DispatchMapTest'` passes, and deleting the new `make_dispatch_table()` row fails the build at the `static_assert`.

## When not to use it

| Situation | Use instead |
| --- | --- |
| Two or three short branches on the key | An `if`/`else if` chain; a table, a lookup function and a fallback cost more to read |
| A closed enum that changes often or has several maintainers | The constexpr table, not the raw jump table; keep the raw table for code generated from the enum itself |
| A closed key set | A table, not a map; a map pays an unknown-key branch on every call |
| A handler needs memory across calls, or a caller swaps it at run time | Strategy, Chapter 8 |
| The outcome depends on the shipment's current status as well as the message type | A state machine, Chapter 10 |
| An action must be undone, queued or replayed | Command, Chapter 11 |

The last three rows are the table's ceiling. A dispatch table answers "which function runs for this key" and nothing more. A third `DeliveryException` for the same tracking id that escalates differently from the first needs memory, so it belongs in an object. A `Delivered` message for a shipment that was `OutForDelivery` is routine, and one for a shipment already `Cancelled` is a carrier error: that's a function of the event and the current state. An operator who marks an exception in error and rolls the ledger entry back needs Command's `execute()`/`undo()`. If any of those holds, write the full pattern and let the table route to it.

## Build, run, observe

From the repository root, the superbuild picks up `examples/25-dispatch-tables`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 cases pass under `cpp23` and 12 under `cpp20`; the two `std::flat_map` cases compile only under C++23. The demo routes messages through all three forms, registers a wire tag after construction and runs the `std::flat_map` variant:

```sh
examples/25-dispatch-tables/demo.sh cpp23
```

```
== Form one: enum-indexed jump table ==
  SH-1001: created: order placed with FastFreight
  SH-1001: picked-up: scanned at origin depot
  SH-1001: delivered: left on porch
  SH-1001: unrecognized message type, raw value ignored

== Form two: dispatch map, open and extensible ==
Built-in wire tags registered: 6
Wire tags registered after onboarding: 7
  SH-2002: created: order placed with RegionalParcel
  SH-2002: in-transit: departed regional hub
  SH-2002: customs-hold: awaiting duty payment
  SH-2002: unrecognized wire tag "UNKNOWN_EVENT", raw message ignored

== Form three: constexpr std::array table ==
  SH-3003: out-for-delivery: on van, last stop of the route
  SH-3003: delivery-exception: recipient refused delivery

== C++23 refinement: the open map as a std::flat_map ==
  SH-4004: delivered: signed for by R. Alvarez
  SH-4004: customs-hold: awaiting duty payment
```

The tag count goes from 6 to 7 when `"CUSTOMS_HOLD"` is registered, and the next dispatch reaches it. Under `cpp20` the `std::flat_map` block is absent.

Two experiments show the difference between the closed-set forms:

- **Swap two jump-table rows.** In a scratch copy, swap `&handle_picked_up` and `&handle_in_transit` in `g_jump_table`. It compiles, and one case fails because two message types now reach each other's handlers.
- **Drop a constexpr row.** In a scratch copy, delete the `Delivered` line from `make_dispatch_table()`. The build stops at the `static_assert`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the Strategy, State and Command entries that dispatch tables stand in for at a smaller scale.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/25-dispatch-tables`, including a gmock collaborator double where an interaction is verified.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
