---
title: "Command, undo, and command queues"
order: 11
part: "selecting-varying-behavior"
description: "Turning a warehouse editor's operations into Command objects with their own execute and undo, an undo/redo stack that steps back and forward through a session, a FIFO queue that defers and batches execution, and the std::function shortcut for a queued action with nothing to undo."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Encapsulate a request as a Command object with an `execute()` and an `undo()`, rather than calling a receiver's method directly from the code that decides an action should happen.
- Build an inverse operation two ways: a clean arithmetic inverse for a symmetric action, and a captured prior-state snapshot for an action with no arithmetic inverse at all.
- Build an undo/redo stack that moves a command between a done stack and an undone stack, and explain why running a new command clears the redo branch.
- Build a FIFO command queue that defers execution and processes a batch of commands in submission order, optionally recording each one into an undo/redo stack as it runs.
- Reach for a `std::function` (or C++23's `std::move_only_function`) instead of a full Command object when a queued action has nothing to undo, and say what that shortcut costs.
- Recognize when Command is solving a problem nobody has yet, and say what to write instead.

## Why this matters

A warehouse inventory and order editor runs the same handful of operations over and over: receive stock into a bin, move stock between bins, correct a quantity after a cycle count, cancel a picked order line. The first version calls a method on the warehouse object directly from wherever the operation is triggered, a keyboard shortcut handler, an HTTP route, a batch-import script, and the call is gone the instant it returns. That works until the editor needs any of three things an inline method call cannot give it: a way to undo the last thing that happened, a way to queue several operations and run them later as a batch, or a way to let several different triggers (a UI button, a scripted import, a scheduled job) submit the same kind of operation without each one re-deriving how to perform it.

Command is the Gang of Four's answer to that force: turn a request into an object instead of a function call. The object remembers everything it needs to run the operation and, critically, everything it needs to run the operation's inverse. Once a request is an object rather than a call already in flight, it can sit in a list waiting to run, travel across a queue to run somewhere else, or sit on a stack waiting to be reversed. None of that is possible with a bare function call, because a function call has no existence after it returns; a Command object does, for as long as something is holding onto it.

This chapter builds that object against a warehouse inventory and order editor. Four operations, adding stock, moving stock, correcting a quantity, and cancelling an order line, become four `InventoryCommand` implementations sharing one receiver, `WarehouseEditor`. An undo/redo stack, `CommandHistory`, lets an operator step backward and forward through a session the way a text editor's Ctrl-Z does. A `CommandQueue` defers a batch of operations, submitted throughout a shift, and runs them in the order they were submitted when a nightly batch job processes them. Not every queued action needs the full ceremony of a Command object with an `execute()` and an `undo()`, so the chapter closes by showing the idiomatic modern shortcut, a plain callable, for a queued action that has nothing to undo, and what that shortcut costs compared to the full object.

{% include excalidraw.html name="11-command-undo" caption="Figure 11.1: An invoker drains a FIFO command queue into a receiver; a CommandHistory moves a command between a done stack and an undone stack as undo() and redo() run." %}

## The domain: editing a warehouse's stock and order lines

The example behind this chapter lives in `examples/11-command`. `WarehouseEditor` is the receiver every command in this chapter mutates: it tracks on-hand quantity per `(bin, sku)` pair and the `OrderLine`s already picked against that stock. It exposes exactly two primitives commands need, `adjust_stock()` and the order-line accessors, and nothing about commands, undo, or queues:

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

`adjust_stock()` takes a signed delta rather than two separate add/remove methods. That one choice is what lets every command in this example express its undo as the identical operation with the sign flipped, instead of writing a second, separately-tested code path for reversing each kind of change. Keeping the receiver this small mirrors the separation Chapter 8 used for `shipping_domain.cpp`'s pure rate math: the receiver knows how to change warehouse state, and the commands below know when and why to call it, with nothing about undo or queueing leaking into the receiver itself.

## The command object: encapsulating a request

Every command in this example implements the same two-method interface:

```cpp
class InventoryCommand {
public:
    virtual ~InventoryCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};
```

`AddStockCommand` is the simplest concrete case. It stores exactly what it needs to run and reverse one stock receipt: a bin, a SKU, and a quantity.

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

The constructor captures the bin, the SKU, and the quantity once; `execute()` calls `editor_.adjust_stock(bin_, sku_, quantity_)` and `undo()` calls the same method with the quantity negated. Nothing about `AddStockCommand` knows whether it is about to run immediately, sit in a queue for an hour, or get replayed by `redo()` after being undone; it only knows how to run itself and how to run its own inverse, which is exactly what makes it safe to hand to an invoker that treats every command the same way, whether that invoker is a `CommandHistory`, a `CommandQueue`, or a caller holding the pointer directly.

`MoveItemCommand` follows the identical shape with two bins instead of one: `execute()` subtracts the quantity from the source bin and adds it to the destination; `undo()` runs both halves in reverse. Both of these commands get a clean, symmetric inverse at no extra design cost, because the operation they perform is itself symmetric: moving a quantity from A to B undoes by moving the same quantity from B back to A.

## Undo, two ways: an inverse operation and a captured snapshot

Not every operation has an arithmetic inverse. "Set this bin's quantity to 12 after a cycle count" has no opposite number to add back; the only way to undo it is to remember what the quantity was before it ran. `AdjustQuantityCommand` captures that prior value inside `execute()`, not the constructor:

```cpp
void AdjustQuantityCommand::execute() {
    previous_quantity_ = editor_.quantity_of(bin_, sku_);
    editor_.adjust_stock(bin_, sku_, new_quantity_ - previous_quantity_);
}

void AdjustQuantityCommand::undo() {
    editor_.adjust_stock(bin_, sku_, previous_quantity_ - new_quantity_);
}
```

Capturing `previous_quantity_` at execute time rather than construction time matters once a command can be queued. A command built now and run an hour later, after a `CommandQueue` has drained several other commands ahead of it, must read the quantity the moment it runs, not the moment somebody wrote `new AdjustQuantityCommand(...)`. Capturing state in the constructor would silently record a stale value if anything else touched that bin in between; capturing it in `execute()` is correct no matter how long the command waited or what ran before it.

`CancelLineCommand` is the same idea against a richer piece of state: an `OrderLine` rather than a single integer. Cancelling a picked line has to return its reserved quantity to the bin it came from and mark the line cancelled; undoing that cancellation has to reverse both parts at once.

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

Capturing the whole `OrderLine` rather than just the quantity means `undo()` restores the line exactly as it was, including a `cancelled` flag that was already true if this line had somehow been cancelled twice. All four commands share the same `InventoryCommand` interface despite the two different undo strategies behind it: the invoker never has to know or care which strategy a given command used, which is the entire point of hiding both behind `execute()`/`undo()`.

## The undo/redo stack

`CommandHistory` owns two stacks of commands, done and undone, and is the piece that makes undo and redo available to a caller:

```cpp
class CommandHistory {
public:
    void execute(std::unique_ptr<InventoryCommand> command);
    bool undo();
    bool redo();
};
```

`execute()` runs the command immediately and pushes it onto the done stack, clearing the undone stack first. `undo()` pops the most recently done command, calls its `undo()`, and moves it onto the undone stack. `redo()` does the mirror image: pop the most recently undone command, call its `execute()` again, and move it back onto the done stack. Clearing the undone stack inside `execute()` is the rule every text editor's undo history already follows: once a new edit happens, whatever you had undone away from is no longer reachable by `redo()`, because redoing it now would silently overwrite the new edit that has already happened in its place. A history that kept the old redo branch around after a new command ran would let `redo()` jump to a state that contradicts whatever the operator just did, which is worse than discarding that branch.

The stack shape also explains why this chapter calls it a stack and not a list: both operations only ever touch the most recent end. `undo()` can only step back one command at a time, in the exact reverse of the order those commands ran, and `redo()` can only step forward through the branch that undo just walked back from. That is the same discipline every mainstream undo implementation uses, from a text editor to a drawing program, because it is the only ordering that keeps every intermediate state consistent: skipping around to an arbitrary earlier command without undoing everything after it first would leave the receiver in a state no sequence of real operations would ever produce.

## The command queue: deferred, batched execution

`CommandHistory` runs a command the instant `execute()` is called. `CommandQueue` is for the opposite case: operations submitted over time that should not run until a batch step decides to process them.

```cpp
class CommandQueue {
public:
    void enqueue(std::unique_ptr<InventoryCommand> command);
    std::size_t process_all(CommandHistory* history = nullptr);
};
```

`enqueue()` only records submission order; nothing runs until `process_all()` drains the queue, FIFO, oldest command first. That ordering guarantee is not cosmetic. If a shift queues an `AddStockCommand` for five units ahead of a `MoveItemCommand` that moves ten units out of the same bin, running them out of order would make the move fail or quietly produce a different final quantity than the warehouse floor intended. FIFO processing is what lets several different triggers, a handheld scanner, a manual correction, a scheduled replenishment job, all queue into the same `CommandQueue` over the course of a shift and trust that a nightly batch step reproduces exactly the sequence of events that happened on the floor.

The optional `CommandHistory*` parameter is what connects the queue back to undo. Pass one in and `process_all()` runs every command through `history->execute()` instead of calling `execute()` directly, so the entire batch remains individually undoable afterward, one command at a time, the same as if an operator had run each one by hand. Pass `nullptr` and the batch is fire-and-forget, appropriate for a queue of actions nobody will ever want to step back through.

## The std::function shortcut and its trade-off

Logging that a restock happened has no undo at all; there is nothing to reverse about having written a line to an audit log. Wrapping that in a class implementing `execute()` and `undo()` would be ceremony with no second method to justify it, the same non-pattern this book has flagged before: when a shape has exactly one real operation, that operation is a function, not an interface. `RestockLogFn` is a type alias for a plain callable instead:

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

Both columns return the same callable shape and the demo calls it the same way either way; only what the callable is allowed to capture changes between standards. The trade-off this shortcut makes is explicit: a `RestockLogFn` has no `undo()`, so it cannot be pushed onto a `CommandHistory` and cannot be stepped backward through. That is the right trade for a side effect with no meaningful reverse, and the wrong one the instant a queued action needs to be undoable, which is exactly the line this chapter draws between the four `InventoryCommand` types above and this one callable.

> **AI sidebar.** Ask an LLM pairing partner for "an undo system" and a common result reaches for a single global stack of closures, each one a lambda that mutates state directly, because that shape is quick to produce and compiles on the first try. Check it the way this chapter trained: a closure-based undo stack works until two different kinds of undo logic need two different kinds of state, at which point every closure needs its own ad hoc capture list, and a reviewer can no longer tell by looking at the stack's declared type what any entry is capable of. An explicit `InventoryCommand` interface costs one extra virtual call per operation and buys a named, swappable, type-checked contract: a function signature a reviewer can read, a queue that can hold commands from multiple unrelated call sites without those sites agreeing on a closure shape in advance, and a place, `undo()`, for the model to be asked directly "does this command's undo actually reverse what execute does," a question a bare lambda has no slot to answer. A generated command slot that is just `std::function<void()>` with a comment reading "assumes caller tracks how to undo this" is the clearest sign the model reached for the closure shape instead of the object shape the forces call for.

## When not to use Command

Command earns its cost when a request needs to outlive the call that created it: because it must be undoable, because it must be queued for later or batched with others, or because more than one trigger submits the same kind of request and none of them should have to re-derive how to perform it. It stops paying for itself the moment none of those three is true.

An operation triggered from exactly one place, that nobody has ever asked to undo, and that always runs immediately, does not need an `InventoryCommand` wrapped around it; it needs a method call. Wrapping `editor.adjust_stock(bin, sku, delta)` in a command object before any of undo, queueing, or multiple triggers is in hand or clearly coming adds a virtual call, a heap allocation for the `unique_ptr`, and a reader's detour through a class definition to find a single call site that will ever use it. The plainer non-pattern is the direct call, the same candidate-non-pattern move Chapter 1 trained for every pattern in this book: name the abstraction and the plain alternative side by side, and let whether a second concern (undo, batching, multiple submitters) is present decide between them.

The `std::function` shortcut earlier in this chapter is itself a smaller version of the same caution in the other direction. Reaching for a full `InventoryCommand` subclass for an action with no undo and no batching requirement, logging an event, sending a notification, firing a webhook, buys nothing a bare callable does not already provide, at the cost of a class definition, a header, and two methods where one function would do. The forces decide which of the two shapes an operation gets, not which one appeared most recently in this chapter.

Part 9 revisits this same shape, a queue of deferred work drained in order, under concurrency: the Active Object pattern decouples a method's invocation from its execution by handing work to a queue that a separate processor thread drains on its own schedule, the same FIFO discipline this chapter's `CommandQueue` uses single-threaded, now running concurrently with whatever submitted the work.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/11-command` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 20 cases under both the `cpp23` and `cpp20` presets. Unlike Chapter 8's `std::expected` split, the C++20/C++23 difference here sits entirely behind the `RestockLogFn` alias rather than behind an extra test case, so the count does not move between presets. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 20   # cpp23
100% tests passed, 0 tests failed out of 20   # cpp20
```

To see the commands, the undo/redo stack, the queue, and the `std::function` shortcut run end to end rather than just reading assertions that say they work, run the example's own demo script:

```sh
examples/11-command/demo.sh cpp23
```

It receives 50 units of `WIDGET-100` into bin `A1`, moves 20 of them to a pick face, undoes and redoes that move, corrects a quantity after a cycle count, cancels and un-cancels an order line, then queues and batch-processes three operations against a second SKU before logging a restock through the `std::function` shortcut:

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

Running `examples/11-command/demo.sh cpp20` prints the same output; the `std::function`/`std::move_only_function` choice behind `RestockLogFn` is an implementation detail that does not change what the demo prints.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Command entry, including the undo-support discussion this chapter's `CommandHistory` directly implements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/11-command`'s suite, which asserts that execute changes state, undo restores it, redo reapplies it, and a FIFO batch lands on the same final state regardless of how many commands ran before it.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
