---
layout: example
order: 25
title: "Shortcut patterns: dispatch tables and maps"
description: Three lightweight stand-ins for a Strategy, State, or Command hierarchy, taught through a shipment-tracking ingestion service.
github_path: examples/25-dispatch-tables
card_eyebrow: "Chapter 25 · Dispatch tables"
card_title: "Routing without a class hierarchy"
card_blurb: "An enum-indexed function-pointer table, an open unordered_map dispatch map, and a compile-time constexpr array, three ways to route a shipment message type to its handler without a full Strategy, State, or Command hierarchy."
---

Three dispatch shortcuts on a shipment-tracking ingestion service:

- **Function-pointer table (jump table).** A `constexpr` array indexed by the
  message-type enum maps each type straight to its handler. The closed-set,
  lowest-overhead form.
- **Dispatch map.** `std::unordered_map<Key, handler>` accepts late
  `register_handler()` calls, the open form for a set that grows at runtime. A
  C++23 variant uses `std::flat_map`.
- **Constexpr table.** A `std::array` table whose completeness is asserted at
  compile time with `static_assert`.

Each form is contrasted against the object-oriented Strategy, State, and Command
designs from earlier chapters, with the teaching point of when a table is enough
and when you have outgrown it.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/25-dispatch-tables/demo.sh cpp23
```

The demo routes a stream of shipment messages through all three forms, shows an
unknown message type falling back to a defined handler rather than crashing, and
registers a new handler into the open map at runtime. The index helper uses
`std::to_underlying` on C++23 and a guarded `static_cast` on C++20. Builds
standalone and under the `cpp20` preset (12 tests).
