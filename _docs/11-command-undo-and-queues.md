---
title: "Command, undo, and command queues"
order: 11
part: "selecting-varying-behavior"
description: "Turning a warehouse editor's operations into Command objects with their own execute and undo, an undo/redo stack that steps back and forward through a session, a FIFO queue that defers and batches execution, and the std::function shortcut for a queued action with nothing to undo."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Encapsulate a request as a Command object with `execute()` and `undo()`, instead of calling the receiver directly from the code that decides an action should happen.
- Build an inverse two ways: an arithmetic inverse for a symmetric action, and a prior-state snapshot for an action with no inverse.
- Build an undo/redo stack, and explain why running a new command clears the redo branch.
- Build a FIFO command queue that defers a batch and can record each command into the undo/redo stack as it runs.
- Use a `std::function`, or `std::move_only_function`, new in C++23, instead of a Command object when a queued action has nothing to undo, and say what that costs.
- Recognize when Command solves a problem nobody has yet, and say what to write instead.

## Why this matters

A warehouse editor runs the same operations all day: receive stock into a bin, move stock between bins, correct a quantity after a cycle count, cancel a picked order line. Called directly from a keyboard handler, an HTTP route or an import script, each operation is gone the moment it returns. That stops working once the editor needs to undo the last change, queue operations for a nightly batch, or accept the same operation from several triggers without each one re-deriving how to perform it.

{% include excalidraw.html name="11-command-undo" caption="Figure 11.1: An invoker drains a FIFO command queue into a receiver; a CommandHistory moves a command between a done stack and an undone stack as undo() and redo() run." %}

## Pattern: Command

### The problem

`WarehouseEditor` is the receiver. It tracks on-hand quantity per `(bin, sku)` and the `OrderLine`s picked against that stock, and it knows nothing about commands, undo or queues:

```cpp
struct OrderLine {
    std::string sku;
    std::string bin;
    int quantity = 0;
    bool cancelled = false;
};

class WarehouseEditor {
public:
    void adjust_stock(const std::string& bin, const std::string& sku, int delta);
    int quantity_of(const std::string& bin, const std::string& sku) const;

    void add_order_line(std::string line_id, OrderLine line);
    OrderLine order_line(const std::string& line_id) const;
    void set_line_state(const std::string& line_id, int quantity, bool cancelled);
};
```

`adjust_stock()` takes a signed delta rather than separate add and remove methods, so each command's undo is the same call with the sign flipped. A method call on this receiver can't be undone, can't wait in a list and can't be submitted from three places as the same kind of thing, because it has no existence after it returns.

### The solution

A **Command** turns a request into an object that holds everything needed to perform it and to reverse it, so it can be queued, logged or undone after the call that created it.

Use a Command when a request must:

- be undone or redone;
- wait in a queue or run in a batch;
- come from several triggers that shouldn't each know how to perform it.

### How C++ models it

Every command implements the same two methods:

```cpp
class InventoryCommand {
public:
    virtual ~InventoryCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};
```

`AddStockCommand` stores what it needs to run and reverse one receipt:

```cpp
class AddStockCommand : public InventoryCommand {
public:
    AddStockCommand(WarehouseEditor& editor, std::string bin, std::string sku, int quantity);
    void execute() override;
    void undo() override;

private:
    WarehouseEditor& editor_;
    std::string bin_;
    std::string sku_;
    int quantity_;
};
```

`execute()` calls `editor_.adjust_stock(bin_, sku_, quantity_)` and `undo()` calls it with the quantity negated. The command doesn't know whether it runs now, waits an hour in a queue or gets replayed by `redo()`. That's what lets any invoker treat every command the same way.

`MoveItemCommand` has the same shape with two bins: `execute()` subtracts from the source and adds to the destination, and `undo()` reverses both. A symmetric operation gets a symmetric inverse with no extra state.

### Undo by snapshot

"Set this bin to 12 after a cycle count" has no opposite number to add back. The only undo is to remember the quantity before it ran. `AdjustQuantityCommand` captures that in `execute()`, not in the constructor:

```cpp
void AdjustQuantityCommand::execute() {
    previous_quantity_ = editor_.quantity_of(bin_, sku_);
    editor_.adjust_stock(bin_, sku_, new_quantity_ - previous_quantity_);
}

void AdjustQuantityCommand::undo() {
    editor_.adjust_stock(bin_, sku_, previous_quantity_ - new_quantity_);
}
```

The timing matters once commands are queued. A command built now and run an hour later, after the queue has drained others ahead of it, must read the quantity when it runs. Captured in the constructor, the snapshot goes stale if anything touches the bin in between.

`CancelLineCommand` snapshots a whole `OrderLine`. Cancelling returns the reserved quantity to its bin and marks the line cancelled; undoing reverses both:

```cpp
void CancelLineCommand::execute() {
    previous_state_ = editor_.order_line(line_id_);
    editor_.adjust_stock(previous_state_.bin, previous_state_.sku, previous_state_.quantity);
    editor_.set_line_state(line_id_, 0, /*cancelled=*/true);
}

void CancelLineCommand::undo() {
    editor_.adjust_stock(previous_state_.bin, previous_state_.sku, -previous_state_.quantity);
    editor_.set_line_state(line_id_, previous_state_.quantity, previous_state_.cancelled);
}
```

| Command | Undo strategy |
| --- | --- |
| `AddStockCommand` | Arithmetic inverse: negate the delta |
| `MoveItemCommand` | Arithmetic inverse: move the quantity back |
| `AdjustQuantityCommand` | Snapshot of the prior quantity, taken in `execute()` |
| `CancelLineCommand` | Snapshot of the prior `OrderLine`, taken in `execute()` |

The invoker never knows which strategy a command uses. Both sit behind `execute()` and `undo()`.

### The undo/redo stack

`CommandHistory` owns a done stack and an undone stack:

```cpp
class CommandHistory {
public:
    void execute(std::unique_ptr<InventoryCommand> command);
    bool undo();
    bool redo();
};
```

`execute()` runs the command, clears the undone stack and pushes the command onto the done stack. `undo()` pops the latest done command, calls its `undo()` and pushes it onto the undone stack. `redo()` is the mirror image. Each call returns `false` when its stack is empty.

Clearing the undone stack is the rule every text editor follows. After a new edit, the branch you had undone away from is gone, because redoing it would overwrite the edit that took its place. Both operations touch only the most recent end, so undo walks back in exact reverse order. Jumping to an arbitrary earlier command without undoing everything after it would leave the receiver in a state no real sequence of operations produces.

### The command queue

`CommandHistory` runs a command the moment it's called. `CommandQueue` holds operations submitted over a shift until a batch step runs them:

```cpp
class CommandQueue {
public:
    void enqueue(std::unique_ptr<InventoryCommand> command);
    std::size_t process_all(CommandHistory* history = nullptr);
};
```

`enqueue()` records submission order, and `process_all()` drains the queue oldest first. The order matters: an `AddStockCommand` for five units queued ahead of a `MoveItemCommand` taking ten out of the same bin runs the move against stock that hasn't arrived yet if they swap, and `adjust_stock()` doesn't refuse a bin going negative. FIFO lets a handheld scanner, a manual correction and a replenishment job share one queue and trust the batch to replay the floor's sequence.

Pass a `CommandHistory*` and `process_all()` runs each command through `history->execute()`, so every command in the batch stays individually undoable. Pass `nullptr` and the batch is fire-and-forget.

The same shape, a queue of deferred work drained in order, returns under concurrency as Active Object, where a separate thread drains the queue.

### The callable shortcut

Logging a restock has nothing to undo. A class with `execute()` and an empty `undo()` would be ceremony around one function, so `RestockLogFn` is a callable. Its definition depends on the standard:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: std::function requires its target to be copy-constructible, even
// though this command slot never actually copies the callable. A lambda
// that captured the unique_ptr directly would make the closure move-only
// and fail to compile, so the receipt is promoted to a shared_ptr first,
// purely to satisfy that requirement.
using RestockLogFn = std::function<void(int, const std::string&)>;

RestockLogFn make_logging_restock(std::unique_ptr<RestockReceipt> receipt) {
    auto shared_receipt = std::shared_ptr<RestockReceipt>(std::move(receipt));
    return [shared_receipt](int quantity, const std::string& bin) {
        shared_receipt->append_line(quantity, bin);
    };
}
```
```cpp
// C++23: std::move_only_function places no copy requirement on its target,
// so the lambda can capture the unique_ptr directly by move, with no
// shared_ptr workaround and no shared ownership where none was wanted.
using RestockLogFn = std::move_only_function<void(int, const std::string&)>;

RestockLogFn make_logging_restock(std::unique_ptr<RestockReceipt> receipt) {
    return [receipt = std::move(receipt)](int quantity, const std::string& bin) mutable {
        receipt->append_line(quantity, bin);
    };
}
```

`std::function` requires a copyable target, so under C++20 a lambda that captures the `unique_ptr` by move fails with `std::function target must be copy-constructible`, and the receipt is promoted to a `shared_ptr` to get around it. `std::move_only_function` drops that requirement. Both return the same callable shape, called the same way.

The trade-off is explicit: a `RestockLogFn` has no `undo()`, so it can't go on a `CommandHistory`. That's right for a side effect with no reverse and wrong the moment a queued action needs undoing.

## When not to use it

- **One trigger, no undo, runs immediately.** Call `editor.adjust_stock(bin, sku, delta)`. A command object adds a virtual call, a heap allocation for the `unique_ptr` and a class to read for a single call site. Introduce Command when undo, batching or a second submitter appears.
- **A side effect with no reverse.** Logging, a notification or a webhook gets a callable, not an `InventoryCommand` subclass with an empty `undo()`.
- **A `std::function<void()>` undo stack.** Closures work until different undo logic needs different state. Then each closure carries an ad hoc capture list, and nothing in the stack's type says what an entry can do. An `InventoryCommand` interface gives every undo a named slot to review.

> **🤖 Explore with your agent**
>
> "In `examples/11-command`, add `WriteOffDamagedCommand` implementing `InventoryCommand`: it removes a damaged quantity from one `(bin, sku)` through `WarehouseEditor::adjust_stock()` with a negative delta, and `undo()` adds it back. Take no snapshot in the constructor; if the command reads editor state, read it in `execute()`. Don't use `std::function` for it, and don't add undo logic to `WarehouseEditor`. Add tests that run it through `CommandHistory` and through `CommandQueue::process_all()` with a history, then undo and redo it, and check the bin's quantity after each step."
>
> Run `ctest --preset cpp23 -R Command` and check that `CommandHistory` and `CommandQueue` are unchanged.

## Build, run, observe

From the repository root, the superbuild picks up `examples/11-command`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 20 cases pass under both presets; the standards differ only behind the `RestockLogFn` alias. To see the commands, the stack, the queue and the shortcut run:

```sh
examples/11-command/demo.sh cpp23
```

It receives 50 units of `WIDGET-100` into bin `A1`, moves 20 to a pick face, undoes and redoes the move, corrects a quantity, cancels and restores an order line, batch-processes three queued operations on `GASKET-7`, then logs two restocks:

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

The `cpp20` build prints the same output.

- **Keep the redo branch.** In a scratch copy, delete `undone_.clear()` from `CommandHistory::execute()`. One case fails: after a new command, `redo()` still reaches the branch it should have discarded.
- **Capture the `unique_ptr` under C++20.** In the C++20 branch of `make_logging_restock`, capture the receipt by move instead of through a `shared_ptr`. The `cpp20` build fails with `std::function target must be copy-constructible`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Command entry, including the undo-support discussion `CommandHistory` implements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/11-command`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).
