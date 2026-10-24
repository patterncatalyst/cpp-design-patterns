# 11 - Command: warehouse inventory and order editing

A warehouse inventory and order editor needs four operations undoable and
one of them batchable: receive stock, move stock between bins, correct a
quantity after a cycle count, and cancel a picked order line. This example
encapsulates each as a Command object with its own `execute()`/`undo()`
pair, an undo/redo stack that can step back and forward through a session,
a FIFO queue that defers and batches commands, and a `std::function`
(C++23: `std::move_only_function`) shortcut for a queued action that has
nothing to undo.

## The domain

- `WarehouseEditor` (`src/warehouse_domain.hpp`): the receiver. Tracks
  on-hand quantity per `(bin, sku)` pair and the `OrderLine`s picked
  against that stock. It knows nothing about commands, undo, or queues;
  it only exposes `adjust_stock()`, `quantity_of()`, and the order-line
  accessors every command composes its `execute()`/`undo()` pair out of.
- Four `InventoryCommand` implementations (`src/command.hpp`):
  - `AddStockCommand` and `MoveItemCommand` undo via a clean arithmetic
    inverse: run the same adjustment with the sign flipped.
  - `AdjustQuantityCommand` and `CancelLineCommand` undo via a captured
    prior-state snapshot, taken inside `execute()` (not the constructor),
    since a queued command runs later than it is built, after other
    commands may have already changed the state it needs to remember.
- `CommandHistory` (`src/command_history.hpp`): the undo/redo stack.
  `execute()` runs a command and pushes it onto the done stack; `undo()`
  pops it, runs its inverse, and moves it to the undone stack; `redo()`
  does the reverse. Running a new command through `execute()` after an
  undo clears the redo stack.
- `CommandQueue` (`src/command_queue.hpp`): a FIFO queue for deferred,
  batched execution. `enqueue()` only records submission order;
  `process_all()` drains the queue in that order, optionally recording
  each command into a `CommandHistory` so the whole batch stays undoable
  afterward.
- `RestockLogFn` (`src/restock_log.hpp`): the `std::function` shortcut.
  Logging a restock event has nothing to undo, so a bare callable is the
  whole answer; a class implementing `execute()`/`undo()` would be
  ceremony with no second method to justify it. Under C++23 this alias is
  `std::move_only_function`, so the callable can capture a move-only
  `std::unique_ptr<RestockReceipt>` directly; under C++20 it is
  `std::function`, which requires its target to be copy-constructible, so
  the receipt is promoted to a `shared_ptr` first purely to satisfy that
  requirement (`src/restock_log.cpp`, guarded by
  `#if __cplusplus >= 202302L`).

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/11-command
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild, from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 20 gtest cases under both the `cpp23` and
`cpp20` presets via `gtest_discover_tests`. Unlike the C++23-guarded
`std::expected` cases in Chapter 8's example, the C++20/C++23 split here
sits entirely behind the `RestockLogFn` alias, not behind an extra test,
so the count does not change between presets. Observed locally on
Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 20   # cpp23
100% tests passed, 0 tests failed out of 20   # cpp20
```

To see the commands, the undo/redo stack, the queue, and the
`std::function` shortcut run end to end rather than just reading
assertions that say they work, run the example's own demo script:

```sh
examples/11-command/demo.sh cpp23
```

Expected output:

```
== Command objects: execute changes state, undo restores it ==
A1/WIDGET-100 after AddStockCommand: 50
A1/WIDGET-100 after MoveItemCommand: 30
PICK-FACE-3/WIDGET-100 after MoveItemCommand: 20
A1/WIDGET-100 after undo: 50
PICK-FACE-3/WIDGET-100 after undo: 0
PICK-FACE-3/WIDGET-100 after redo: 20

== A cycle-count correction and a cancelled order line ==
PICK-FACE-3/WIDGET-100 after cycle-count correction: 12
PICK-FACE-3/WIDGET-100 after cancelling LINE-9001: 17
PICK-FACE-3/WIDGET-100 after undoing the cancellation: 12

== Command queue: deferred, batched, FIFO execution ==
Pending before processing: 3
Processed in submission order: 3
B2/GASKET-7 after the batch: 25
PICK-FACE-1/GASKET-7 after the batch: 10

== std::function shortcut: a queued action with nothing to undo ==
GASKET-7: +30 into B2
GASKET-7: +5 into B2
```

Running `examples/11-command/demo.sh cpp20` prints the same output; the
`std::function`/`std::move_only_function` choice is an implementation
detail behind `RestockLogFn` and does not change what the demo prints.

## Files

```
examples/11-command/
  CMakeLists.txt         # standalone + superbuild-integrated targets
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py             # standalone Conan recipe (gtest/1.17.0)
  demo.sh                  # build + run the demo binary
  src/
    warehouse_domain.hpp/.cpp   # WarehouseEditor receiver + OrderLine
    command.hpp/.cpp            # InventoryCommand + 4 concrete commands
    command_history.hpp/.cpp    # CommandHistory undo/redo stack
    command_queue.hpp/.cpp      # CommandQueue FIFO batch processor
    restock_log.hpp/.cpp        # std::function/move_only_function shortcut
    demo_main.cpp                # exercises all of the above
  tests/
    command_test.cpp
    command_history_test.cpp
    command_queue_test.cpp
    restock_log_test.cpp
```
