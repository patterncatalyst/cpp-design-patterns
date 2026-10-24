# 04 — Interface Segregation + Dependency Inversion: order notifications

An order fulfillment system needs to tell a customer when an order ships,
runs late, or arrives, and it needs to do that over whichever channel that
customer is reachable on: email, SMS, or a channel not yet written. This
example builds that notification path two SOLID principles at a time: a
fat "gateway" interface split into focused role interfaces (ISP), and two
high-level policies that depend only on those abstractions, wired to
concrete gateways by hand in one composition root (DIP), with no DI
container anywhere.

## The domain

- `OrderEvent` — `order_id`, `customer_contact` (an email address or a
  phone number, the struct does not care which), and `message_body`.
- `DispatchResult` — a `DispatchStatus` (`Sent` or `Failed`) plus a
  `dispatch_id`.
- `format_message()` in `src/notification_domain.cpp` is the one shared
  formatting rule both gateways below call into, so an identical event
  reads identically no matter which channel sent it. Only the transport
  differs between gateways, the same shared-core move Chapter 8 used for
  shipping-rate math.

## Interface segregation: three role interfaces, not one fat one

`src/gateway_interfaces.hpp` declares three narrow interfaces instead of
one `NotificationGateway` interface with every method any channel might
ever need:

- `MessageSender` — `send(const OrderEvent&) -> DispatchResult`.
- `TemplateRenderer` — `render(template_id, const OrderEvent&) -> string`.
- `DeliveryStatusProvider` — `status_of(dispatch_id) -> DispatchStatus`.

Two concrete gateways implement different subsets:

- `EmailGateway` (`src/email_gateway.hpp`) implements all three, because
  it has all three capabilities.
- `SmsGateway` (`src/sms_gateway.hpp`) implements only `MessageSender`.
  It never declares `TemplateRenderer` or `DeliveryStatusProvider` as a
  base, so it is never asked to fake a template renderer or a status
  lookup it cannot really provide. `tests/gateways_test.cpp` proves this
  at compile time with `static_assert(!std::is_base_of_v<...>)`.

## Dependency inversion: three high-level policies, one abstraction each

- `OrderNotificationPolicy` (`src/order_notification_policy.hpp`) depends
  on `MessageSender&` only. Either gateway satisfies it; the class never
  names `EmailGateway` or `SmsGateway`.
- `TemplatedOrderNotifier` depends on `TemplateRenderer&` and
  `MessageSender&`, exactly the two capabilities it needs.
- `DeliveryAuditor` depends on `DeliveryStatusProvider&` only.

Every dependency is a **reference**, not a `std::unique_ptr`: ownership of
the concrete gateways stays with whoever composes the object graph, not
with the policy that borrows a role out of it. `src/demo_main.cpp` is that
composition root, the one place that names both the abstractions and
their concrete implementations and wires them together by hand. Nothing
else in `src/` ever names a concrete gateway type.

### C++23 refinement (guarded)

`OrderNotificationPolicy::try_notify_customer()` returns
`std::expected<DispatchResult, std::string>` under C++23, rejecting an
event with no `customer_contact` with a typed reason instead of a
sentinel or an exception. Under C++20 the same method returns
`std::optional<DispatchResult>`, which loses the reason. Guarded with
`#if __cplusplus >= 202302L` in `src/order_notification_policy.hpp` and
`.cpp`, so the mandatory C++20 build path never requires
`std::expected`. The matching gtest cases in
`tests/order_notification_policy_test.cpp` are guarded the same way.

## Test doubles: hand-written fakes, not a mocking framework

`tests/fakes.hpp` has three hand-written fakes, `FakeMessageSender`,
`FakeTemplateRenderer`, and `FakeDeliveryStatusProvider`, each
implementing exactly one role interface. `FakeMessageSender` records every
event it is asked to send so a test can assert on what the policy under
test actually sent. No mocking framework is involved: a hand-written fake
is enough to prove a high-level policy only ever talks to the abstraction
it was given, which is the point this example exists to demonstrate.

## Build, run, observe

No containers — local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/04-solid-dependency-inversion
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild — from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 14 gtest cases under both `cpp23` and
`cpp20` via `gtest_discover_tests` (the two `try_notify_customer` cases
compile against `std::expected` under `cpp23` and against
`std::optional` under `cpp20`, so the count does not change between
standards, only the error-handling idiom under test does). Observed
locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 14   # cpp20
```

To see the composition root wire two gateways into three high-level
policies and exercise all of them, run the example's own demo script:

```sh
examples/04-solid-dependency-inversion/demo.sh cpp23
```

Expected output:

```
SMS dispatch (sms-ORD-1001): [Order ORD-1001] Your order has shipped.
Email dispatch (email-ORD-1002): [Order ORD-1002] We're sorry, your order is running late. it left the warehouse this morning
Audit for email-ORD-1002: confirmed delivered

== C++23 refinement: typed validation via std::expected ==
Rejected event: order event has no customer_contact to notify
```

Running `examples/04-solid-dependency-inversion/demo.sh cpp20` prints the
same output minus the final `std::expected` block, which only compiles
under the `cpp23` preset.

## Files

```
examples/04-solid-dependency-inversion/
  CMakeLists.txt         # standalone + superbuild-integrated targets (solid2_ prefix)
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    notification_domain.hpp/.cpp    # OrderEvent, DispatchResult, shared format_message()
    gateway_interfaces.hpp          # MessageSender, TemplateRenderer, DeliveryStatusProvider
    email_gateway.hpp/.cpp          # concrete: implements all three role interfaces
    sms_gateway.hpp/.cpp            # concrete: implements MessageSender only
    order_notification_policy.hpp/.cpp   # DIP: depends on MessageSender
    templated_order_notifier.hpp/.cpp    # ISP: depends on TemplateRenderer + MessageSender
    delivery_auditor.hpp/.cpp            # ISP: depends on DeliveryStatusProvider
    demo_main.cpp                   # composition root: wires concrete gateways to policies
  tests/
    fakes.hpp                        # hand-written test doubles, one per role interface
    order_notification_policy_test.cpp
    templated_order_notifier_test.cpp
    delivery_auditor_test.cpp
    gateways_test.cpp                # incl. static_assert proof of ISP segregation
```
