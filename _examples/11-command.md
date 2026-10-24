---
layout: example
order: 11
title: "Command, undo, and queues"
description: A warehouse editor whose operations are command objects, with undo/redo and a FIFO command queue.
github_path: examples/11-command
card_eyebrow: "Chapter 11 · Command"
card_title: "Warehouse editor: Command + undo"
card_blurb: "Inventory operations encapsulated as command objects, with two undo strategies, an undo/redo stack, a FIFO command queue, and a std::function shortcut."
---

A warehouse inventory editor whose operations are command objects:

- **Command objects.** `AddStockCommand` and `MoveItemCommand` undo with a clean
  arithmetic inverse; `AdjustQuantityCommand` and `CancelLineCommand` undo from
  a snapshot captured inside `execute()` (not the constructor, since a queued
  command runs later than it is built).
- **`CommandHistory`** is the undo/redo stack; a fresh execute clears the redo
  branch.
- **`CommandQueue`** is a FIFO batch processor that can keep a batch undoable.
- **`RestockLogFn`** is the `std::function` shortcut for a queued action with
  nothing to undo.

The C++23 codetabs path uses `std::move_only_function` so the command slot can
hold a move-only capture; the C++20 path uses `std::function`.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 20/20 tests pass
./examples/11-command/demo.sh cpp23
```

The demo executes a mix of commands, undoes and redoes them, drains a FIFO
batch, and writes restock-log entries. Chapter 11 walks through the two undo
strategies and when a plain callable beats a full command object. Builds
standalone and under the `cpp20` preset (20 tests).
