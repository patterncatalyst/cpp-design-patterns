---
title: "SOLID II: interface segregation, dependency inversion, and DI without a framework"
order: 4
part: "principles"
description: "Segregating a fat interface into focused role interfaces, inverting the dependency between a high-level policy and a low-level implementation, and composing the resulting object graph by hand, in idiomatic C++ with no DI container."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Recognize the fat interface that the Interface Segregation Principle targets, and split it into role interfaces.
- State the Dependency Inversion Principle: the high-level policy and the low-level detail both depend on an abstraction.
- Inject dependencies through constructors with no framework, wired in one composition root.
- Choose between owning an injected dependency (a smart pointer) and borrowing it (a reference), from the lifetimes involved.
- Judge when a DI container such as Boost.DI is worth adding.

## Why this matters

An order fulfillment system tells a customer when an order ships, runs late or arrives. The account on file may take email or SMS, and another channel will come later. What changes is the delivery mechanism, not the decision to notify.

The naive design has one `NotificationGateway` with methods to send, render a template and check delivery status, and every channel implements all three. SMS can't render a template and has no status API, so `SmsGateway` either throws from those methods or returns a fake answer. That's a Liskov violation (Chapter 3) manufactured by the interface. The Interface Segregation Principle (ISP) fixes it one level up: no client should be forced to depend on, implement or fake a method it doesn't use.

{% include excalidraw.html name="04-solid-dependency-inversion" caption="Figure 4.1: DIP inverts the dependency arrow from a high-level policy through an abstraction, not straight to a concrete gateway; ISP splits that abstraction into role interfaces a concrete type implements only where it has the capability." %}

The Dependency Inversion Principle (DIP) answers which way the dependency points. In Robert C. Martin's words, high-level modules shouldn't depend on low-level modules; both should depend on abstractions, and abstractions shouldn't depend on details. `OrderNotificationPolicy` depends on `MessageSender`, and `EmailGateway` implements `MessageSender`. The arrow that would run from the policy down to a concrete gateway now stops at an interface the high-level side owns.

## The domain: notifying a customer about an order

The code is in `examples/04-solid-dependency-inversion`. `OrderEvent` carries an order id, a `customer_contact` (an email address or a phone number; the struct doesn't care which) and a `message_body`. `DispatchResult` reports a `DispatchStatus`, `Sent` or `Failed`, and a `dispatch_id`.

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

Every channel builds its text with one shared function, `format_message()`, so a wording fix lands everywhere and two channels always agree on content.

## The smell: one interface, every capability

```cpp
class NotificationGateway {
public:
    virtual ~NotificationGateway() = default;
    virtual DispatchResult send(const OrderEvent& event) const = 0;
    virtual std::string render(const std::string& template_id, const OrderEvent& event) const = 0;
    virtual DispatchStatus status_of(const std::string& dispatch_id) const = 0;
};
```

`EmailGateway` has all three capabilities. An SMS carrier API sends short plain text and reports nothing back, so `SmsGateway` has two bad options:

| `SmsGateway` does | Consequence |
| --- | --- |
| throws from `render()` and `status_of()` | every holder of a `NotificationGateway&` must know the concrete type before calling, which defeats the interface |
| returns a default: `""` from `render()`, `DispatchStatus::Failed` from `status_of()` | callers get answers that look valid and aren't |

## Segregating into role interfaces

`src/gateway_interfaces.hpp` declares three narrow interfaces, each named for one promise:

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

| Type | Implements |
| --- | --- |
| `EmailGateway` | all three |
| `SmsGateway` | `MessageSender` only |

A type may satisfy several role interfaces; ISP only says it mustn't be forced to satisfy one it can't. `SmsGateway` has no `render()` to stub, and no caller can ask an SMS channel to render, because the type system never offers the call. A `static_assert(!std::is_base_of_v<TemplateRenderer, SmsGateway>)` turns that into a build failure if someone adds the capability back.

Segregation also narrows each client's dependency. `DeliveryAuditor` only checks whether a dispatch landed, so it takes a `DeliveryStatusProvider`, and its test double implements one method instead of three.

## Dependency inversion: who depends on whom

`OrderNotificationPolicy` is the high-level policy: it knows an order event goes to a customer, and nothing about the channel.

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

Both depend only on `MessageSender`; only the failure path of `try_notify_customer` differs between standards. The policy has no `#include "email_gateway.hpp"` and no `std::make_unique<EmailGateway>`. Its tests link against a short hand-written fake instead of an email client. That's what DIP buys: the `#include` graph between the orchestration layer and the plumbing is reversed, so the orchestration layer builds and tests on its own.

### Own or borrow

The policy holds `MessageSender&`, not `std::unique_ptr<MessageSender>`:

| Hold the dependency as | When |
| --- | --- |
| a reference | the composition root outlives every consumer, and the dependency is wired once at startup |
| `std::unique_ptr` | the holder replaces the dependency at run time and so manages its lifetime, like Chapter 8's `RateEstimator` and its strategy |
| `std::shared_ptr` | ownership is shared by several holders with independent lifetimes |

## Dependency injection without a framework

C++ has no reflection and no container that scans a project and wires constructors. The idiom is a composition root: one function, usually `main()`, that constructs each concrete type once and passes it to whatever constructor needs the role. `src/demo_main.cpp` is that root:

```cpp
EmailGateway email_gateway;
SmsGateway sms_gateway;

OrderNotificationPolicy sms_notifier(sms_gateway);
TemplatedOrderNotifier email_notifier(email_gateway, email_gateway, "order-delayed");
DeliveryAuditor auditor(email_gateway);
```

- It's the only file that constructs a concrete gateway; the three policy headers name only abstractions.
- `email_gateway` goes into `TemplatedOrderNotifier` twice, once as `TemplateRenderer&` and once as `MessageSender&`, because it's the one type that fills both roles. `SmsGateway` can't be wired there: it isn't a `TemplateRenderer`, so the compiler rejects it.
- There's no registration step. Every wire is visible reading top to bottom.

### When a container helps

| A DI container such as Boost.DI pays when | Manual wiring wins when |
| --- | --- |
| the graph is large enough that wiring it by hand is its own maintenance burden | the graph is wired once at startup and reads top to bottom |
| bindings vary by build, such as fakes in a test binary and real gateways in production, from the same registration code | one `main()` per binary is enough |
| lifetimes need scoping rules, such as one instance per request | everything lives as long as the program |

A container also costs a pinned and audited dependency, a binding DSL every new team member has to learn, and distance between "this type is constructed" and the line that constructs it. For a graph like this one, and for many production services, the manual composition root is the better call.

<blockquote class="ai-sidebar" markdown="1">
**Specifying dependency injection.** "Add DI to `OrderNotificationPolicy`" doesn't say how many interfaces, who owns each collaborator, or where wiring happens. State those:

> In `examples/04-solid-dependency-inversion`, add a push-notification channel as `PushGateway` implementing only `MessageSender` from `src/gateway_interfaces.hpp`. Don't add an interface per class, a registration map or a container. Policies keep taking role interfaces by reference (`MessageSender&`, `TemplateRenderer&`, `DeliveryStatusProvider&`), not `std::shared_ptr`. Construct `PushGateway` once in `src/demo_main.cpp` next to `EmailGateway` and `SmsGateway`, and don't touch `OrderNotificationPolicy`.

Check with `ctest --preset cpp23`, and reject a diff that edits `src/order_notification_policy.hpp` or makes `PushGateway` derive from `TemplateRenderer`.
</blockquote>

## When not to invert

| Principle | Worth it when | Skip it when |
| --- | --- | --- |
| DIP | a second implementation is in hand or coming, or a test must replace a network call, a clock or a carrier API with a fake | one channel, no second one planned, nothing to fake: call the class by name |
| ISP | clients need different subsets, as `SmsGateway` and `EmailGateway` do | every remaining method is used by every client; the interface is complete, not fat |

An abstraction with one implementation adds a virtual call, a header and a detour for the reader. Splitting a two-method interface that every client uses in full gives a maze of one-method types. Name the abstraction and the plain alternative side by side, as Chapter 1 does for Strategy, and let the forces decide.

## Build, run, observe

From the repository root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 cases pass under both presets: `try_notify_customer` compiles against `std::expected` under C++23 and `std::optional` under C++20, so no case is compiled out. Then run the demo:

```sh
examples/04-solid-dependency-inversion/demo.sh cpp23
```

It sends an SMS, sends a templated email, audits the email's delivery, and shows the `std::expected` rejection:

```
SMS dispatch (sms-ORD-1001): [Order ORD-1001] Your order has shipped.
Email dispatch (email-ORD-1002): [Order ORD-1002] We're sorry, your order is running late. it left the warehouse this morning
Audit for email-ORD-1002: confirmed delivered

== C++23 refinement: typed validation via std::expected ==
Rejected event: order event has no customer_contact to notify
```

Under `cpp20` the demo prints the same output without the final `std::expected` block.

- **Give SMS a renderer.** In a scratch copy, declare `class SmsGateway : public MessageSender, public TemplateRenderer`. The build fails: the `static_assert` on `TemplateRenderer` fires, and `SmsGateway` becomes abstract wherever it's constructed.

## For deeper coverage

- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the original statement of the Dependency Inversion Principle and the Interface Segregation Principle this chapter builds on, alongside the SRP/OCP/LSP trio Chapter 3 covers.
- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the book's foundational advice to "program to an interface, not an implementation," which DIP and ISP both sharpen into concrete, testable rules.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
