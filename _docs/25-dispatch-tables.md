---
title: "Shortcut patterns: dispatch tables and maps"
order: 25
part: "patterns-in-the-wild"
description: "Three lightweight alternatives to a full Strategy, State, or Command class hierarchy, taught against a shipment-tracking ingestion service: an enum-indexed function-pointer table (the classic jump table), a std::unordered_map dispatch map for an open, runtime-extensible set, and a constexpr std::array table checked complete at compile time."
duration: "25 min"
---

## Learning objectives

By the end of this chapter you can:

- Build an enum-indexed function-pointer table, the classic jump table, and explain why its correctness depends on a positional convention nothing in the type system enforces.
- Build a dispatch map over `std::unordered_map` for an open, runtime-extensible set of keys, and register a new handler into it after the program has already started.
- Build a `constexpr std::array` dispatch table that assigns each handler to an explicit, enumerator-derived index and proves the table complete with a `static_assert`, catching a missing entry at compile time instead of at a crash.
- Name the one real difference `std::to_underlying` makes over a bare `static_cast` when converting an enum to a table index, and when a `std::flat_map` is the better choice over a node-based map for a write-rarely, read-often table.
- Decide, for a given set of message or event kinds, whether a table or a map is the right-sized tool, or whether the set has outgrown it and needs the full Strategy, State machine, or Command machinery this book already built.
- Recognize when a jump table or dispatch map is solving a problem a single `if`/`else if` chain already solves at three message kinds, and say what to write instead.

## Why this matters

A shipment-tracking service sits behind a handful of carrier webhooks, and every inbound message carries a type tag: a parcel was created, picked up, moved to a hub, loaded for the final leg, delivered, or bounced back as undeliverable. Something has to look at that tag and run the one function that knows what to do with it. The least disciplined way to write that something is a chain of `if (type == CarrierMessageType::Created) { ... } else if (type == CarrierMessageType::PickedUp) { ... }` that keeps growing sideways as message kinds are added, with every branch paying the cost of every branch that came before it in the worst case and a reviewer's attention stretched across a function that does six unrelated things.

The Gang of Four answer to "behavior that varies with a changing condition" is a full pattern: the Strategy pattern for an algorithm chosen by a caller, the State pattern and its finite-state-machine idioms for behavior chosen by a current status, the Command pattern for a request turned into an object with its own `execute()` and `undo()`. Those answers earn their weight when the behavior behind each branch is substantial: a `PaidState` with real invariants to protect, a `MoveStockCommand` that has to compute and remember its own inverse. A message-type dispatcher that calls one short function per tag and otherwise does nothing clever is a different, smaller problem, and reaching for a full class hierarchy to solve it is often over-engineering in the other direction: a `CreatedMessageHandler` class with a constructor, a virtual `handle()` override, and a factory to build it, for a function that is three lines long and never varies at runtime.

This chapter builds the lightweight answer to that smaller problem three idiomatic ways, then names exactly where each one stops being enough and a full pattern from Chapters 8, 10, or 11 takes over. An enum-indexed function-pointer table, the classic jump table, resolves a known tag to a handler in one array lookup, no branching at all. A dispatch map over `std::unordered_map` does the same job for a set of keys this codebase does not fully control, letting a new carrier's message kind register a handler at runtime with no recompile. A `constexpr std::array` table gets the jump table's speed back while fixing its biggest weakness: a table built at compile time can be checked for completeness at compile time, turning a missing handler into a build failure instead of a null function pointer waiting to crash in production.

{% include excalidraw.html name="25-dispatch-tables" caption="Figure 25.1: a carrier webhook feed routes through one of three dispatch mechanisms (jump table, dispatch map, constexpr table), closed-set forms on the left, the open, runtime-extensible map on the right, all three funneling into shared handler functions and a TrackingLedger." %}

## The domain: routing a carrier webhook feed

The example behind this chapter lives in `examples/25-dispatch-tables`. A shipment-tracking ingestion service receives messages from more than one carrier, and this chapter models two distinct shapes those messages can take. `CarrierMessage` carries a `CarrierMessageType`, a closed enum this codebase compiles against directly:

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

`CarrierWireMessage` carries a `wire_tag`, a plain string standing in for whatever a third-party carrier's webhook actually sends on the wire, a value this codebase has no control over and cannot enumerate ahead of time:

```cpp
struct CarrierWireMessage {
    std::string wire_tag;
    std::string tracking_id;
    std::string detail;
};
```

Both shapes resolve to the same six handler functions, `handle_created`, `handle_picked_up`, and so on through `handle_delivery_exception`, each writing one note into a shared receiver, `TrackingSink`, the interface `TrackingLedger` implements. Keeping the handler bodies identical across all three dispatch forms is deliberate, the same discipline interchangeable Strategy expressions rely on: if the routing mechanism changes, the behavior behind each route should not, and the only way to make that true by construction rather than by care is to have every mechanism call the same functions.

## Form one: the enum-indexed jump table

The jump table is as direct as dispatch gets: an array of function pointers, one slot per enumerator, indexed by converting the enum to an integer:

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

Dispatch here costs one bounds check and one indexed call through a raw function pointer, no virtual table, no hash computation, nothing allocated. That is the entire case for this form: when the set of message kinds is fixed and known at the point this array is written, nothing beats an array lookup for speed, and nothing is easier for a reviewer who already knows the enum to read. The bounds check on `index` matters for a reason that has nothing to do with C++ semantics: a wire value deserialized from an untrusted network payload can carry any integer, not just one of the six this enum declares, and `static_cast`ing an out-of-range integer to an enum class and using it to index an array is undefined behavior the moment nobody checks the range first.

What this form does not buy you is any protection against the one mistake that breaks it silently. `g_jump_table`'s six entries have to stay in the exact order `CarrierMessageType`'s six enumerators were declared in, because `message_type_index()` is nothing more than the enumerator's position reinterpreted as an array subscript. Insert a seventh message kind, `ReturnedToSender`, in the middle of the enum instead of at the end, and forget to insert its row in the same position in `g_jump_table`, and every handler from that point on now answers for the wrong message type. The compiler says nothing. The test suite might catch it if it happens to exercise the shifted rows, and it might not. This is the jump table's whole trade: maximal speed and minimal code, paid for with a convention a human has to maintain by discipline, not by a rule the compiler checks.

`message_type_index()` is the one place this chapter's two closed-set forms show a real standard difference, not a stylistic one:

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

Neither version changes behavior for `CarrierMessageType`, whose underlying type defaults to `int` and whose six small values convert to `std::size_t` without surprise either way. The value of `std::to_underlying` grows with an enum that declares an explicit, narrower underlying type, where a bare `static_cast` to the target type silently does two conversions in one step and a reader has to know the enum's declaration to see that. `examples/25-dispatch-tables` guards the two versions behind `#if __cplusplus >= 202302L` inside `message_type_index()` itself in `src/carrier_message.hpp`, so both the jump table and the constexpr table below call the identical function and get the identical answer under either standard.

## Form two: the dispatch map, open and extensible

A `wire_tag` arriving from an actual carrier webhook is not one of six values this codebase enumerated ahead of time. It is a string, and a new carrier integrated next quarter will send strings nobody wrote `CarrierMessageType::ReturnedToSender` to anticipate. `MessageDispatchMap` answers that with a `std::unordered_map` instead of an array:

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

`register_handler()` is the whole point of choosing a map over an array: a caller reaches it after construction, at any point while the program is already running, and the dispatch map's behavior changes with no recompile and no change to `MessageDispatchMap`'s own source. `examples/25-dispatch-tables`'s demo registers a handler for `"CUSTOMS_HOLD"`, a wire tag none of the six built-in rows above know about, well after `MessageDispatchMap` is constructed, and the very next `dispatch()` call routes to it correctly. That late-binding property is exactly what the self-registering Prototype registry buys for a different kind of key, a carrier policy name instead of a wire tag; the difference here is that registration happens explicitly, at a call site a developer can see, rather than as a static-initialization side effect in each concrete type's own translation unit. Either mechanism keeps the set open; which one fits depends on whether the caller registering a handler is this codebase's own code (where self-registration saves a step) or an operator onboarding a new carrier at runtime (where an explicit `register_handler()` call is the clearer story).

An unknown `wire_tag`, one that was never seeded and never registered, resolves to `handlers_.end()` and falls back to `handle_unrecognized_wire_tag()`, which records the raw tag for whoever reviews the ledger later rather than crashing or silently dropping the message. That fallback matters more for the open form than for either closed-set form: a closed enum at least guarantees every value a caller can construct corresponds to something the dispatch mechanism was built to handle, modulo the out-of-range cast the jump table guards against. A string key has no such guarantee at all, so every dispatch map needs a defined answer for "I have never seen this key," not as an edge case to patch in later but as a first-class branch of `dispatch()` from the start.

### A C++23 refinement: the open table as a flat map

`MessageDispatchMap` registers a handful of handlers once, typically at startup or when a new carrier is onboarded, and dispatches on every single inbound message after that: a write-rarely, read-often profile. `std::unordered_map`'s node-based buckets are not built for that profile; each entry is a separate heap allocation, and a lookup pays a hash computation plus a pointer chase into whichever bucket that hash lands in. C++23's `std::flat_map` stores its keys and values in two contiguous, sorted vectors instead, trading an amortized O(1) hash lookup for an O(log n) binary search over memory a CPU can actually prefetch:

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

`examples/25-dispatch-tables`'s `FlatMessageDispatchMap`, guarded behind `#if __cplusplus >= 202302L` in `src/dispatch_map.hpp` and `.cpp`, is the identical class with that one member's type changed; `register_handler()` and `dispatch()` are unmodified. The mandatory C++20 build path never includes `<flat_map>` and never instantiates this type, so the compatibility preset is never at risk of a header that does not exist for it.

## Form three: the constexpr array table, checked complete

The jump table's weak point was never its speed, it was the silent risk of a table that drifts out of sync with the enum it dispatches on. `make_dispatch_table()` closes that gap by building the same closed-set table inside a `constexpr` function, assigning each handler to an index computed from the enumerator itself rather than from its position in a list:

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

Two things changed from the jump table, and both matter. First, every assignment names the enumerator it is filling a slot for, so reordering the lines inside `make_dispatch_table()` changes nothing: `table[message_type_index(CarrierMessageType::Delivered)]` finds the right slot no matter where that line sits relative to the others, which removes the exact failure mode that makes the jump table fragile. Second, the whole table is built once, at compile time, inside a `constexpr` function, which means a `static_assert` can read every slot before the program exists at all. `std::array` default-initializes its `MessageHandler` slots to `nullptr`, so a forgotten line here is not a wrong answer, it is a null entry, and `std::ranges::none_of` walking the finished table at compile time turns that null entry into a build failure with a message naming the problem, not a runtime crash the first time a message of that type happens to arrive in production.

`dispatch_via_constexpr_table()` reads almost identically to the jump table's dispatch function, because the mechanism underneath, an array indexed by a converted enum, is the same one:

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

The bounds check for an out-of-range `index` is still necessary here for the same reason it was necessary for the jump table: `static_assert` proves every declared enumerator has a handler, it says nothing about a garbled integer arriving from outside the enum's declared range. What `static_assert` buys back is the thing the jump table could never guarantee: that every value the enum itself declares is covered, checked once, at compile time, rather than trusted to a reviewer's memory every time the enum grows.

## When a table outgrows itself: Strategy, state machines, and Command

All three forms in this chapter share a ceiling. They answer "which function runs for this key," and nothing more. The moment the answer to that question needs more than a function call, a table is the wrong tool, and the full pattern it was standing in for is the right one.

Strategy is the right tool again the moment a handler needs to hold state across calls or needs to be swapped by a caller at runtime rather than selected by an incoming key. A dispatch table's handlers are stateless functions chosen once per call by a key that comes with the message; a `RateStrategy` is an object a caller owns, configures, and can replace mid-session without touching the message format at all. If a carrier's `DeliveryException` handling needs to remember how many exceptions a given tracking id has already triggered, so a third exception escalates differently than a first one, that memory belongs in an object, not in a static table row, and Strategy is where that object goes.

The State pattern and its transition-table and `std::variant` idioms become the right tool again when "what happens next" depends on more than the message's own type: it depends on what the shipment's current status already is. This chapter's dispatch forms treat every `CREATED` message identically regardless of what came before it. A real shipment lifecycle cares whether a `Delivered` message arrives for a shipment that was `OutForDelivery` (expected) or one that was already `Cancelled` (a carrier error to flag, not a routine update). That is a function of two things, the incoming event and the current state, which is precisely the shape finite-state-machine idioms are built for. A dispatch table indexed only by message type has no slot for "and what was the shipment's status a moment ago"; reach for a state machine the moment that question matters.

Command is the right tool again when a dispatched action needs to be undone, queued, or replayed independently of the message that triggered it. This chapter's handlers run once and return nothing a caller can later reverse. If recording a `DeliveryException` needs an undo path, because an operator sometimes marks an exception in error and needs to roll the ledger entry back, that need is Command's `execute()`/`undo()` contract, not a function pointer. The `std::function` shortcut for a queued action with nothing to undo is a closer cousin to this chapter's dispatch map than Command's full object is; the distinguishing question is always the same one Command turns on: does this action ever need to be reversed, and is reversing it cheap to express as "run the same operation with the sign flipped," or does it need a dedicated inverse captured at the moment the action ran.

The decision procedure this chapter trains is small enough to apply in a code review: count the distinct behaviors a dispatch key selects among, and ask whether any of them need memory across calls, awareness of a changing status, or an undo path. Zero to that whole question and the message type alone decides everything: a table or a map is enough, and reaching further costs more than it returns. Any yes, and the table was never going to carry the real requirement; write the Strategy, the state machine, or the Command object the requirement actually needs, and let this chapter's dispatch forms route to it rather than replace it.

> **AI sidebar.** Ask an LLM pairing partner to "add a dispatch table for shipment events" and a common result reaches straight for `std::map<EventType, std::function<...>>` even when the set of event types is small, fixed, and known at the call site, which is exactly the case this chapter's jump table and constexpr table both handle faster and with less machinery. `std::function` type-erases a callable behind a heap-allocated wrapper in the general case, a cost worth paying for the dispatch map's open set, where the handler behind a given key is unknown until runtime, and a cost paid for nothing when the set is closed and every handler is a plain function known at compile time. The same generated code often skips the completeness check entirely, building a `std::unordered_map` by hand with one `insert()` call per enumerator and no mechanism that notices when a new enumerator's `insert()` call was never written. Read a generated dispatch table the way this chapter trained: ask whether the key set is open or closed before accepting the container choice, and ask what happens, today, if a case is missing without a trace. If nothing stops that, the jump table or the constexpr table's `static_assert` is the fix, not a bigger map.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/25-dispatch-tables` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset, 12 under `cpp20`, the difference being the two `std::flat_map`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see all three forms route real messages, see the closed-set unknown-type fallback fire, and see the dispatch map accept a late registration and route to it immediately, run the example's own demo script:

```sh
examples/25-dispatch-tables/demo.sh cpp23
```

It dispatches a short sequence of messages through the jump table, the dispatch map (including a wire tag registered after construction), and the constexpr table, then the C++23 `std::flat_map` variant:

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

Notice the wire tag count climbing from 6 to 7 between the two dispatch-map lines: `"CUSTOMS_HOLD"` was never one of the six built-in rows, and the very next `dispatch()` call after `register_handler()` routes to it correctly, with no restart and no recompile. Running `examples/25-dispatch-tables/demo.sh cpp20` prints the same output minus the `std::flat_map` block, which only compiles under the `cpp23` preset.

## When not to use it

A dispatch table or map is worth the extra code the moment a chain of `if`/`else if` branches on the same key would otherwise grow past three or four cases, because at that size a table reads as data a reviewer can scan in one glance, while an `if` chain reads as code a reviewer has to trace branch by branch. Below that size, the `if` chain is not a problem to fix; two or three branches, each a short, self-contained check, cost a reader less than a table declaration, a lookup function, and an unrecognized-key fallback would. Introducing a `MessageDispatchMap` for two message kinds is solving a problem of scale that has not arrived yet, the same caution that applies to every pattern in this book: match the mechanism to the force actually in front of you, not to the force a textbook example implies.

The jump table specifically should not be reached for when the enum it indexes changes often or is maintained by more than one person without a shared review habit; the positional-ordering risk this chapter walked through is a real cost, not a theoretical one, and the constexpr array table removes that exact risk for a one-time cost of writing `make_dispatch_table()` instead of a bare initializer list. Prefer the constexpr form by default for any closed set a team maintains together, and reserve the raw jump table for a context where the array is generated or otherwise guaranteed never to drift, such as code generated directly from the enum's own declaration.

The dispatch map's openness is a cost as well as a feature. An open map cannot tell a caller, at compile time, whether a key it is about to dispatch exists; every single call pays the branch for "what if this key was never registered," a branch the two closed-set forms only pay once, at the boundary where an untrusted integer enters the system. Reach for the map specifically when the key set is actually open, a third party controls part of it, or a plugin-style registration step is a real requirement, not merely because `std::unordered_map` is a familiar, flexible-looking container. And reach past all three forms in this chapter, back to Strategy, a state machine, or Command, the moment a handler needs memory across calls, needs to know a changing status rather than just an incoming message type, or needs an undo path; this chapter's tables and maps answer "which function runs," not "what does an evolving, stateful object do."

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, *Design Patterns: Elements of Reusable Object-Oriented Software* (Addison-Wesley): the Strategy, State, and Command entries this chapter's three dispatch forms stand in for at a smaller scale, and the source to reach for the moment a handler needs more than a function call.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/25-dispatch-tables`'s suite, which asserts that every closed-set form routes every message type to its own handler, that an out-of-range or unregistered key falls back rather than crashing, and that a dispatch map accepts and immediately uses a handler registered after construction, backed by a gmock collaborator double where the test verifies an interaction rather than reading state back.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
