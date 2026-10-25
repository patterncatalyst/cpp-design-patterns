---
title: "SOLID II: interface segregation, dependency inversion, and DI without a framework"
order: 4
part: "principles"
description: "Segregating a fat interface into focused role interfaces, inverting the dependency between a high-level policy and a low-level implementation, and composing the resulting object graph by hand, in idiomatic C++ with no DI container."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Recognize the fat-interface smell the Interface Segregation Principle targets, and split it into focused role interfaces instead.
- State the Dependency Inversion Principle precisely: both the high-level policy and the low-level detail depend on an abstraction, and explain why that is a stronger claim than "use an interface."
- Build constructor injection with no framework: a high-level policy holds a reference to an abstraction, and one composition root wires concrete types to it.
- Decide whether an injected dependency should be owned (a smart pointer) or merely borrowed (a reference), and justify the choice from the lifetimes involved rather than from habit.
- Judge when a DI container such as Boost.DI is worth its added complexity and when manual wiring is the stronger engineering call.
- Recognize when segregating an interface or inverting a dependency solves a problem nobody has yet, and say what to write instead.

## Why this matters

An order fulfillment system has to tell a customer when an order ships, runs late, or arrives, and the way it reaches that customer is not fixed: an account on file might take email, a phone number might take SMS, and a channel that does not exist yet will exist eventually. Chapter 3 named the force behind SRP, OCP, and LSP as "this will change, and the code around it should not have to." Here the thing that changes is the delivery mechanism, not the decision to notify, and that split motivates the two principles this chapter adds to Chapter 3's three: Interface Segregation (ISP) and Dependency Inversion (DIP).

The naive shape for this system reaches for one interface, something like `NotificationGateway`, with a method for sending, a method for rendering a template, and a method for checking delivery status, and asks every channel to implement all three. SMS cannot render a template the way email can, and it has no delivery-status API to poll, so an `SmsGateway` built against that fat interface either throws from the methods it cannot honor or returns a silent no-op. Chapter 3 named that exact failure mode under a different principle: a type that cannot honor its base class's full contract violates Liskov Substitution the moment a caller assumes the contract holds. ISP is the fix one level upstream of LSP's symptom: a client should never be forced to depend on, implement, or fake a method it does not use. Segregate the fat interface into `MessageSender`, `TemplateRenderer`, and `DeliveryStatusProvider`, and `SmsGateway` only ever declares the one capability it has.

{% include excalidraw.html name="04-solid-dependency-inversion" caption="Figure 4.1: DIP inverts the dependency arrow from a high-level policy through an abstraction, not straight to a concrete gateway; ISP splits that abstraction into role interfaces a concrete type implements only where it has the capability." %}

Dependency Inversion is the second half of the same picture, and it answers a different question: not what shape the abstraction takes, but which direction the dependency arrow points. Without DIP, `OrderNotificationPolicy`, the high-level object that decides an order event is worth telling a customer about, would `#include` a concrete `EmailGateway` directly, and every unit test for that policy would need a real or stubbed email client just to link. DIP's statement, Robert C. Martin's own formulation, is that high-level modules should not depend on low-level modules; both should depend on an abstraction, and the abstraction should not depend on the details. `OrderNotificationPolicy` depends on `MessageSender`. `EmailGateway` also depends on `MessageSender`, implementing it rather than being depended on directly. The dependency that used to run from policy to concrete gateway now runs from both of them toward the interface, and the interface sits conceptually with the policy, not with the gateway. That is the inversion: the arrow that would naturally point from the orchestrating code down into implementation detail is redirected to stop at a boundary the high-level side defines.

## The domain: notifying a customer about an order

The example behind this chapter lives in `examples/04-solid-dependency-inversion`. `OrderEvent` carries an order ID, a `customer_contact` (an email address or a phone number, the struct does not know or care which), and a `message_body`. `DispatchResult` reports back a `DispatchStatus`, `Sent` or `Failed`, and a `dispatch_id`.

```cpp
struct OrderEvent {
    std::string order_id;
    std::string customer_contact;
    std::string message_body;
};

struct DispatchResult {
    DispatchStatus status;
    std::string dispatch_id;
};
```

One shared function, `format_message()`, assembles the text every channel actually sends, the same shared-core move Chapter 8 uses for its three pricing strategies: keep the one fact that must stay consistent in one place, so a wording fix lands everywhere at once and a test can assert that two different channels agree on content without an epsilon or a special case.

## The smell: one interface, every capability

The fat interface this example avoids looks like this, and it is worth writing out once so the segregated version on the other side of it reads as a fix rather than an arbitrary style choice:

```cpp
class NotificationGateway {
public:
    virtual ~NotificationGateway() = default;
    virtual DispatchResult send(const OrderEvent& event) const = 0;
    virtual std::string render(const std::string& template_id, const OrderEvent& event) const = 0;
    virtual DispatchStatus status_of(const std::string& dispatch_id) const = 0;
};
```

An `EmailGateway` built against this has all three capabilities, so implementing it costs nothing extra. An `SmsGateway` does not: an SMS carrier API sends a short plain-text message and reports nothing else back. Forced to implement `NotificationGateway` anyway, `SmsGateway` has exactly two choices, both bad. It can throw from `render()` and `status_of()`, which means every caller that holds a `NotificationGateway&` now has to know, out of band, which concrete type it actually has before calling those two methods safely, defeating the entire point of programming against an interface. Or it can return a harmless-looking default, an empty string from `render()` or `DispatchStatus::Failed` from `status_of()` regardless of what actually happened, which is worse: the caller gets an answer that looks valid and is not. Either way, the fat interface has manufactured a Liskov violation that segregation would have made structurally impossible, because there would be nothing to stub.

## Segregating into role interfaces

`src/gateway_interfaces.hpp` declares three narrow interfaces instead, each named for the one thing it promises:

```cpp
class MessageSender {
public:
    virtual ~MessageSender() = default;
    virtual DispatchResult send(const OrderEvent& event) const = 0;
};

class TemplateRenderer {
public:
    virtual ~TemplateRenderer() = default;
    virtual std::string render(const std::string& template_id, const OrderEvent& event) const = 0;
};

class DeliveryStatusProvider {
public:
    virtual ~DeliveryStatusProvider() = default;
    virtual DispatchStatus status_of(const std::string& dispatch_id) const = 0;
};
```

`EmailGateway` implements all three, because it has all three capabilities; nothing about ISP says a type cannot satisfy more than one role interface, only that no type should be forced to satisfy a role it does not have. `SmsGateway` implements exactly `MessageSender`. It never writes `class SmsGateway : public TemplateRenderer`, so there is no `render()` to stub, no `status_of()` to fake, and no caller that can accidentally call either one on an SMS channel, because the type system never offers that call in the first place. `tests/gateways_test.cpp` turns that absence into a compile-time assertion: `static_assert(!std::is_base_of_v<TemplateRenderer, SmsGateway>)` fails to compile the moment someone "helpfully" adds the capability back without updating the type, which is a stronger guarantee than a code review catching the same regression after the fact.

Segregation also changes who needs to depend on what. A `DeliveryAuditor` that only ever checks whether a dispatch landed has no business depending on `MessageSender` or `TemplateRenderer` at all; against the fat interface it would have had to, and its own unit test would have needed a fake that implements `send()` and `render()` just to satisfy the type, methods the auditor's logic never calls. Against `DeliveryStatusProvider` alone, the auditor's test double implements one method, and the dependency the auditor declares in its constructor signature is an accurate description of what it needs.

## Dependency inversion: who depends on whom

`OrderNotificationPolicy` is the high-level policy in this example: it knows an order event is worth notifying a customer about, and it knows nothing else. It depends on `MessageSender`, not on `EmailGateway` or `SmsGateway`:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: try_notify_customer loses the rejection reason behind
// std::optional. The caller learns *that* an event was rejected,
// not *why* it failed validation.
class OrderNotificationPolicy {
public:
    explicit OrderNotificationPolicy(MessageSender& sender) : sender_(sender) {}

    DispatchResult notify_customer(const OrderEvent& event) const {
        return sender_.send(event);
    }

    std::optional<DispatchResult> try_notify_customer(const OrderEvent& event) const {
        if (event.customer_contact.empty()) {
            return std::nullopt;
        }
        return sender_.send(event);
    }

private:
    MessageSender& sender_;
};
```
```cpp
// C++23: std::expected carries the rejection reason alongside the
// success value, checkable with an if and no exception machinery.
class OrderNotificationPolicy {
public:
    explicit OrderNotificationPolicy(MessageSender& sender) : sender_(sender) {}

    DispatchResult notify_customer(const OrderEvent& event) const {
        return sender_.send(event);
    }

    std::expected<DispatchResult, std::string> try_notify_customer(const OrderEvent& event) const {
        if (event.customer_contact.empty()) {
            return std::unexpected<std::string>("order event has no customer_contact to notify");
        }
        return sender_.send(event);
    }

private:
    MessageSender& sender_;
};
```

Both columns depend on exactly `MessageSender`. Only the failure path of `try_notify_customer` changed between standards, the same pattern Chapter 8 used for a rejected shipment: the inversion itself, the choice of what `OrderNotificationPolicy` is allowed to name, is identical in both columns and does not move when the standard does.

Notice what is absent from `OrderNotificationPolicy`: no `#include "email_gateway.hpp"`, no `std::make_unique<EmailGateway>` anywhere in this class, and no compile-time link between this policy and any specific channel's implementation. A unit test for this policy links against a hand-written fake that implements `MessageSender` in twelve lines, not against a real or stubbed email client. That is the concrete, measurable payoff DIP buys: the direction of the `#include` graph between the orchestrating layer and the plumbing layer gets reversed, and reversing it is what makes the orchestrating layer testable and reusable in isolation from the plumbing.

The dependency in `OrderNotificationPolicy` is held as a reference, `MessageSender&`, not a `std::unique_ptr<MessageSender>`. That is a deliberate contrast with Chapter 8's `RateEstimator`, which owned its strategy through a `unique_ptr` because a `RateEstimator` needed to reassign, and therefore manage the lifetime of, a new strategy at runtime. `OrderNotificationPolicy` never reassigns its sender; the object that composes the graph owns the concrete gateways for as long as the program runs, and every policy that borrows a role out of that graph can hold a plain reference instead of a smart pointer. Reach for ownership (`unique_ptr` or `shared_ptr`) when the dependency's lifetime needs to be managed independently of its caller, the way a runtime-swappable strategy does; reach for a reference when the composition root and every consumer share one lifetime, which is the common case for a dependency wired once at startup.

## Dependency injection without a framework

C++ ships no reflection, no annotation processor, and no runtime container that scans a project and wires constructors together, the way a managed-language framework might. The idiomatic answer is not to build one; it is to write a composition root: one function, usually `main()` or a thin wrapper around it, that constructs every concrete type exactly once and passes each abstraction's concrete implementation to whatever constructor asks for it. `src/demo_main.cpp` is that composition root for this example:

```cpp
EmailGateway email_gateway;
SmsGateway sms_gateway;

OrderNotificationPolicy sms_notifier(sms_gateway);
TemplatedOrderNotifier email_notifier(email_gateway, email_gateway, "order-delayed");
DeliveryAuditor auditor(email_gateway);
```

Three things stand out in those five lines. First, this is the only file in the whole example that constructs a concrete gateway type; `order_notification_policy.hpp`, `templated_order_notifier.hpp`, and `delivery_auditor.hpp` each depend only on the abstraction they need. Second, `email_gateway` is passed in twice, once as a `TemplateRenderer&` and once as a `MessageSender&`, because it is the one concrete type in this graph that satisfies both roles; `TemplatedOrderNotifier`'s constructor does not know or care that both references happen to resolve to the same object. Third, there is no container, no registration step, and no startup phase that resolves a dependency graph by reflection; the graph is exactly as deep as these five lines make it, and a reader can trace every wire by reading top to bottom once.

This is also the place to weigh when a DI container such as Boost.DI helps instead of fighting the language. A container pays for itself when the graph is large enough that hand-wiring it becomes its own maintenance burden, when bindings vary by build configuration (a test binary wiring in fakes, a production binary wiring in real gateways, from the same registration code), or when object lifetimes need scoping rules, such as one instance per request, that a flat `main()` cannot express cleanly. None of that is true of this chapter's five-object graph, and it will not be true of most services this book's size. A container also costs something real: another dependency to pin and audit, a templated binding DSL a new team member has to learn before they can read `main()`, and a level of indirection between "this type is constructed" and "here is the line that constructs it" that a debugger's stack trace does not always make obvious. For a graph wired once at startup and read start to finish, the manual composition root is not a simplification made for a book example; it is very often the better engineering call even in production code, and reaching for a container by default, before the graph is actually large enough to need one, trades a problem you do not have for a dependency you do.

> **AI sidebar.** Ask an LLM pairing partner to "add dependency injection" to a class and it will often generate one interface per concrete class, one `std::shared_ptr` per member regardless of whether ownership is shared, and sometimes a hand-rolled container with a registration map, because that shape appears constantly in tutorials for languages that do have reflection-based containers. Check the result against the forces this chapter named: an interface is warranted when a second implementation exists or is clearly coming, or when a test needs to substitute a fake, not merely because "DI" was mentioned. A generated `IEmailGateway` with exactly one implementation and no second one on the roadmap is premature abstraction with an interface-shaped costume on. The same scrutiny applies to ownership: a generated constructor parameter list that is `shared_ptr` for every single collaborator, including ones with no real shared-ownership story, is usually a model defaulting to the safest-looking choice rather than reasoning about who actually owns what; a plain reference is correct, and cheaper to read, whenever the composition root outlives every consumer.

## When not to invert

Both principles in this chapter earn their cost only under specific conditions, and both stop paying for themselves well before those conditions arrive.

Dependency Inversion is worth the abstraction when more than one implementation is in hand or clearly coming, or when a test needs to substitute a fake for something a concrete type would make awkward or slow, a network call, a clock, a real carrier API. A service with exactly one notification channel, no second one on any roadmap, and no test that would benefit from faking it does not need a `MessageSender` interface; it needs a function, or a class called directly by name. Introducing an abstraction there adds a virtual call, an extra header, and a reader's detour through an interface to find the one implementation that will ever exist, with nothing on the other side of that cost to justify it. That is the same candidate-non-pattern move Chapter 1 trained for Strategy: name the abstraction and the plain alternative side by side, and let the forces decide, not the principle's reputation.

Interface Segregation has a matching failure mode in the other direction: splitting every method of a two-method interface into its own single-method interface because "narrow is good" produces a maze of one-method types a reader has to chase across files to understand what a class actually does, with no client ever benefiting from the extra seams because every client already needed every method. Segregate when two or more clients have different needs, the way `SmsGateway` and `EmailGateway` differ in this chapter's example; stop segregating once every remaining method is used by every remaining client, because at that point the "fat" interface is not fat, it is simply complete, and splitting it further is churn dressed up as principle.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/04-solid-dependency-inversion` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 14 cases under both the `cpp23` and `cpp20` presets, because `try_notify_customer`'s two test cases compile against `std::expected` under one standard and `std::optional` under the other rather than being compiled out entirely. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 14   # cpp20
```

To see the composition root wire two gateways into three high-level policies and exercise all of them, rather than just reading an assertion that says the wiring works, run the example's own demo script:

```sh
examples/04-solid-dependency-inversion/demo.sh cpp23
```

It prints an SMS dispatch, a templated email dispatch, an audit check against the email gateway's delivery log, and finally the C++23 `std::expected` rejection path:

```
SMS dispatch (sms-ORD-1001): [Order ORD-1001] Your order has shipped.
Email dispatch (email-ORD-1002): [Order ORD-1002] We're sorry, your order is running late. it left the warehouse this morning
Audit for email-ORD-1002: confirmed delivered

== C++23 refinement: typed validation via std::expected ==
Rejected event: order event has no customer_contact to notify
```

Running `examples/04-solid-dependency-inversion/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the Dependency Inversion Principle and the Interface Segregation Principle this chapter builds on, alongside the SRP/OCP/LSP trio Chapter 3 covers.
- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the book's foundational advice to "program to an interface, not an implementation," which DIP and ISP both sharpen into concrete, testable rules.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
