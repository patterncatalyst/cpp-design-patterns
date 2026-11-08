---
title: "Seams and the test safety net"
order: 27
part: "changing-inherited-code"
description: "Getting inherited C++ under test before changing it: characterization tests pin a shipment-SLA checker that reads the wall clock, an object seam makes the clock substitutable so the deadline boundary can be pinned and corrected, and a link seam replaces a free function's grace-period policy at link time."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Write a characterization test that pins what inherited code does now, including behavior that looks wrong.
- Recognize a hard dependency by its symptom: a test that widens its margins or skips a case because it can't control a collaborator.
- Introduce an object seam with a small interface and constructor injection, and confirm the extraction alone changed nothing.
- Use GoogleMock through the seam to pin a boundary that was unreachable before.
- Introduce a link seam for a free function by compiling two definitions of one declaration into two binaries.
- Say why the preprocessor seam is a last resort.
- Apply the order: characterize, then seam, then change behavior with every test green throughout.

## Why this matters

Inherited code wasn't written with your next change in mind, and by the time that change arrives, shipments and customers depend on the code doing exactly what it does. Changing it without a test is a bet that you foresaw every consequence. Michael Feathers defines legacy code as code without tests, whatever its age or quality, because the missing test is what makes a change risky.

A useful test needs two things much inherited code refuses to give: a way to observe the result and a way to control the inputs. The shipment-SLA checker below decides whether a shipment has missed its promised delivery window, and it asks the operating system for the time inside the method that does the comparison. That one choice makes its boundary behavior impossible to test. The job isn't to rewrite the check. It's to make the check controllable, confirm nothing moved, then fix the one wrong behavior.

{% include excalidraw.html name="27-seams" caption="Figure 27.1: the same inherited dependency made substitutable three ways, an object seam for a constructor-injected Clock, a link seam for a free function's grace-period policy, with the preprocessor seam held in reserve." %}

## Characterization tests: pinning what the code actually does

A **characterization test** records what code does, not what it should do. You run the code, copy down the answer, and assert it, so a later edit is checked against the recorded answer rather than against memory. It pins questionable behavior as found, such as an off-by-one at a boundary. You can't decide whether a quirk is a bug until you can reproduce it, and you can't fix it safely until the behavior around it is pinned too.

`LegacyShipmentSlaChecker::is_breached()` takes a `ShipmentSla`, a tracking ID paired with a promised deadline, and reports whether the deadline has passed:

```cpp
bool LegacyShipmentSlaChecker::is_breached(const ShipmentSla& sla) const {
    return std::chrono::system_clock::now() > sla.deadline;
}
```

A deadline well in the past gives `true`, one well in the future gives `false`, and both are easy to pin:

```cpp
TEST(LegacyShipmentSlaCheckerCharacterizationTest, DeadlineWellInThePastIsBreached) {
    LegacyShipmentSlaChecker checker;
    const ShipmentSla sla{"SHM-9001", std::chrono::system_clock::now() - std::chrono::hours(2)};

    EXPECT_TRUE(checker.is_breached(sla));
}
```

The two-hour margin is a symptom of the hard dependency. `is_breached()` reads `system_clock::now()` a few microseconds after the caller read it to build the deadline. With a tight margin, scheduling jitter between the two reads could flip the answer, and you'd be measuring the operating system instead of the checker. The wide margin avoids that without fixing it.

The case the legacy tests can't write at all is a deadline exactly equal to the current moment: two separate calls to `now()` never land on the same nanosecond. That's the checker's most important boundary, the instant the deadline arrives, and it can't be observed until the code changes shape. A boundary the characterization tests can't reach marks where the seam goes.

## The object seam: making a dependency substitutable

A **seam**, in Feathers' terms, is a place where you can change behavior without editing the code where that behavior lives. An **object seam** passes the dependency in from outside, through a constructor or setter, behind a small interface, so production code supplies the real implementation and a test supplies a substitute.

`ShipmentSlaChecker` is the legacy checker refactored through an object seam. The clock read moves out of the method and becomes a `Clock` taken by reference:

```cpp
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::chrono::system_clock::time_point now() const = 0;
};

class ShipmentSlaChecker {
public:
    explicit ShipmentSlaChecker(const Clock& clock) : clock_(clock) {}
    bool is_breached(const ShipmentSla& sla) const;
private:
    std::reference_wrapper<const Clock> clock_;
};
```

`SystemClock`, the only `Clock` the shipped binary constructs, forwards to the system clock, as the legacy checker did inline. A `ShipmentSlaChecker` built with a `SystemClock` gives the same answers through one extra virtual call. Prove that before anything else: run the same two-hour-margin scenarios against the seamed checker with a real `SystemClock`, and they give the same `true` and `false`.

The seam changes what a test can control. `MockClock`, a GoogleMock subclass of `Clock`, reports whatever moment you choose, so the unreachable boundary becomes three instant, deterministic cases: exactly at the deadline, one nanosecond before and one nanosecond after.

```cpp
TEST(ShipmentSlaCheckerTest, DeadlineExactlyAtNowIsBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(::testing::Return(deadline));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9103", deadline};

    EXPECT_TRUE(checker.is_breached(sla));
}
```

There's no sleeping and no margin. `EXPECT_CALL` fixes the one value `now()` returns, and the result is checked against a deadline chosen to the nanosecond. An object seam lets you drive a specific code path on command, not only avoid the real collaborator.

## Changing behavior once the seam exists

The legacy comparison `now() > deadline` treats a deadline equal to the current moment as not yet breached. Nobody could confirm that before the seam, because the boundary was unreachable. With `MockClock` it's an ordinary case, and the answer is clear: the instant the deadline arrives is the instant the SLA is violated. The strict comparison was never a design choice.

`ShipmentSlaChecker::is_breached()` makes the fix: `clock_.get().now() >= sla.deadline`. It's safe because the tests run throughout. The wide-margin cases show behavior away from the boundary didn't move, and the three boundary cases pin the new rule. Put `>` back and exactly one case fails, the deadline-equals-now case, which a two-hour margin would never notice.

`LegacyShipmentSlaChecker` stays as found, still answering `false` at the exact boundary. It's the frozen record the characterization tests pinned, and a reviewer can diff the two checkers to see the one line that changed.

## The link seam: a dependency with no object to inject into

An object seam needs a class whose constructor a test can call. `late_fee_cents()` is a bare free function from a billing batch job, called from many sites, that turns minutes late into a fee in cents:

```cpp
int late_fee_cents(int minutes_late) {
    const int grace_minutes = static_cast<int>(grace_period().count());
    const int effective_minutes = minutes_late - grace_minutes;
    if (effective_minutes <= 0) {
        return 0;
    }
    return effective_minutes * kCentsPerLateMinute;
}
```

`grace_period()` is the hard dependency: the enterprise policy for how many minutes of grace a shipment gets before fees accrue. No constructor exists to pass a different grace period into. Adding a grace-period parameter is the right long-term fix, Feathers' Parameterize Method, but if the function has forty call sites in a batch job you don't yet understand, changing every caller is a bigger and riskier edit than adding one test.

A **link seam** uses the linker's resolution of a free function instead. `grace_period()` is declared once:

```cpp
// grace_policy.hpp
std::chrono::minutes grace_period();
```

It's defined twice: `grace_policy_production.cpp` returns the 15-minute enterprise policy, and `tests/grace_policy_test_double.cpp` returns 0 minutes. `late_fee_schedule.cpp` compiles once into the `seam27_core` object library and links into both binaries unchanged. Each CMake target picks the definition by the file it compiles alongside the core: `seam27_demo` compiles the production policy, `seam27_tests` the 0-minute double.

The results prove the substitution happened. Five minutes late costs 750 cents in `seam27_tests`, where the production 15-minute grace would charge nothing; the demo binary charges 0 cents for the same five minutes. Production pays nothing for the seam in its source or its signatures. The cost is a second small translation unit and a build rule that puts exactly one definition in each binary. Put both in one binary and the link fails at once with `multiple definition of 'seam27::grace_period()'`.

## The preprocessor seam: a last resort

Feathers describes a third seam for when neither of the others is available: a build that can't easily swap translation units, or a header-only function with no `.cpp` to choose between. A **preprocessor seam** wraps the dependency in conditional compilation, so test and production builds contain different code at that point:

```cpp
#ifdef SLA_DISABLE_PENALTY_LOGGING
inline void log_penalty_assessed(const ShipmentSla&) { /* no-op in the stripped test build */ }
#else
inline void log_penalty_assessed(const ShipmentSla& sla) { enterprise_audit_log::record(sla); }
#endif
```

A test that passes with `SLA_DISABLE_PENALTY_LOGGING` defined proves something about that build only. The shipped binary runs different instructions at that line, so a bug in the stripped branch is invisible to every test against the guarded build. Object and link seams keep one body of compiled logic and change only what it's handed; the preprocessor seam gives that up to work anywhere. Use it only where nothing else reaches, and replace it with an object or link seam once you understand the surrounding code.

## The safety-net discipline, in order

The order is fixed:

1. **Characterize.** Pin what the code does now, wide margins and unreachable boundaries included, before touching anything.
2. **Introduce a seam.** Extract an interface, inject it, or swap a linked translation unit, whichever fits the dependency, and confirm with the characterization tests that the seam alone changed nothing.
3. **Change behavior.** With every test green against the seam, make the change, add a test that pins the corrected behavior where it lives, and watch everything else stay green.

In order, the three steps turn "I'm fairly sure this still works" into "every test that passed before still passes, plus one new test for what I meant to change."

> **🤖 Explore with your agent**
>
> "In `examples/27-seams`, keep `LegacyShipmentSlaChecker` unchanged as the characterized reference. Add a `Clock` interface with a pure virtual `now()` and a `SystemClock` that forwards to `std::chrono::system_clock::now()`. Give `ShipmentSlaChecker` an `explicit` constructor taking `const Clock&`, stored as `std::reference_wrapper<const Clock>`. Don't intercept `now()` with macros or link tricks. Write a GoogleMock `MockClock` and pin `is_breached()` exactly at the deadline and one nanosecond either side, with the deadline counted as breached (`>=`). Leave `late_fee_cents()`'s signature alone and break its `grace_period()` dependency with a link seam: one declaration in `grace_policy.hpp`, a production definition for `seam27_demo` and a 0-minute test double for `seam27_tests`."
>
> Check the reply with `ctest --preset cpp23 -R 'ShipmentSlaChecker|LateFeeSchedule'`, and confirm `grace_policy_production.cpp` isn't in the `seam27_tests` sources.

## Build, run, observe

From the repository root, the superbuild picks up `examples/27-seams`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 11 tests pass under both presets. To see the legacy checker, the seamed checker and the link-seamed fee schedule side by side, run the demo:

```sh
examples/27-seams/demo.sh cpp23
```

```
== Legacy checker: hard dependency on the wall clock, no seam ==
legacy SHM-7001 breached: true
legacy SHM-7002 breached: false

== Seamed checker: Clock injected through the constructor ==
seamed SHM-7001 breached: true
seamed SHM-7002 breached: false

== Late fee schedule: grace period reached through the link seam ==
5 minutes late: 0 cents (within production's 15-minute grace)
25 minutes late: 1500 cents
```

Five minutes late costs nothing here because the demo links `grace_policy_production.cpp` and its 15-minute grace; `seam27_tests` links the 0-minute double and charges 750 cents. The cpp20 demo prints the same output.

Two experiments, each in a scratch copy:

- Change `>=` back to `>` in `ShipmentSlaChecker::is_breached()`: one of the 11 tests fails, the deadline-exactly-at-now case.
- Add `src/grace_policy_production.cpp` to the `seam27_tests` sources: the link fails with `multiple definition of 'seam27::grace_period()'`.

`ShipmentSlaChecker::try_time_remaining()` is the one piece that differs between standards; the demo never calls it. It serves a caller, such as a billing job, that needs the magnitude as well as the yes-or-no: minutes remaining, or minutes overdue. `std::optional` can only report that the deadline has passed. `std::expected`, new in C++23, keeps the overdue amount in its error channel:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: std::optional can only report that the deadline has passed,
// not by how much; the overdue amount is discarded the moment it slips.
std::optional<std::chrono::minutes> try_time_remaining(const ShipmentSla& sla) const {
    const auto now = clock_.get().now();
    if (now >= sla.deadline) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::minutes>(sla.deadline - now);
}
```
```cpp
// C++23: std::expected carries the overdue amount in its error channel
// instead of discarding it, giving a caller such as a billing job both
// the yes/no answer and the magnitude in one call.
std::expected<std::chrono::minutes, std::chrono::minutes> try_time_remaining(const ShipmentSla& sla) const {
    const auto now = clock_.get().now();
    if (now >= sla.deadline) {
        return std::unexpected<std::chrono::minutes>(
            std::chrono::duration_cast<std::chrono::minutes>(now - sla.deadline));
    }
    return std::chrono::duration_cast<std::chrono::minutes>(sla.deadline - now);
}
```

Both versions use the same `Clock` seam and the same `MockClock`. Only the shape of the breached answer changes.

## When not to use it

A seam has a cost. An object seam adds an interface and a virtual call, and a reader has to look elsewhere to find which `Clock` a given checker runs against. A link seam adds a translation unit and a build rule a maintainer must understand before adding a caller. Neither is large; pay them when a test needs them.

| Situation | Do this |
| --- | --- |
| Small unit, one call site, intended behavior you can write down in full from memory | Rewrite it and test the rewrite against its intended contract |
| Behavior other systems depend on in ways nobody can list from reading the code | Characterize, seam, then change |
| A value computed once at startup with no substitution need | Leave it; an interface here is speculative generality |
| No other way for a test to observe or control the dependency | Object seam first, link seam for a free function, preprocessor seam last |

One question separates the first two rows: can you list every behavior the unit must preserve, confident nobody will find an exception next week? If yes, rewrite it. If no, that uncertainty is what a characterization test resolves.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the original definition of a seam, the full catalogue of dependency-breaking techniques, the source of the object, link, and preprocessor seams, and the fuller discussion of characterization tests as a discipline distinct from specification testing.
- Hyuk Myeong, *GoogleTest in Practice* (self-published): the GoogleMock idioms behind `MockClock` and `EXPECT_CALL`, read alongside the official [GoogleTest documentation](https://google.github.io/googletest/).
- The full reading list is on the [bibliography page]({{ '/bibliography/' | relative_url }}).
