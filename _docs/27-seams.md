---
title: "Seams and the test safety net"
order: 27
part: "changing-inherited-code"
description: "Getting inherited C++ code under test before changing it, taught against a shipment-SLA checker that reads the wall clock directly. Characterization tests pin the checker's current behavior, including a boundary the hard dependency makes untestable, an object seam makes the clock substitutable so the boundary can be pinned exactly and corrected under test, and a link seam breaks a free function's dependency on an enterprise grace-period policy with no object to inject anything into."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Write a characterization test that pins what a piece of inherited code actually does right now, including behavior that looks wrong, as distinct from a specification test that asserts what the code should do.
- Recognize a hard dependency, code that reaches directly into a concrete collaborator it cannot be told to substitute, and name the specific symptom it produces: a test that has to widen its margins or skip a case entirely because it cannot control the dependency.
- Introduce an object seam by extracting a small interface and injecting an implementation through a constructor, and prove the extraction alone changed nothing observable before changing anything else.
- Use GoogleMock to supply a controlled double through an object seam and pin a boundary condition that was unreachable before the seam existed.
- Introduce a link seam for a dependency with no natural object to inject anything into, by declaring a function once and compiling two definitions of it into two different binaries.
- Explain why the preprocessor seam is a last resort, what it costs even when it is the only option, and when reaching for it is still the right call.
- Apply the safety-net discipline in order: characterize first, then introduce a seam, then change behavior with the test suite staying green the entire time.

## Why this matters

Code you inherit was not written with your next change in mind. It was written to satisfy whatever requirement was in front of its author at the time, under whatever time pressure was in front of them, and it got checked in once it worked. Nobody asked at the time whether a future maintainer would need to substitute one of its collaborators for a test double; the question did not exist yet, because the change that will need the substitution has not been imagined. By the time that change arrives, the code has usually been running in production for months or years, with real shipments or real customers depending on it continuing to do exactly what it already does. Changing it without a test watching is a bet that you understood every consequence of the change before you made it, a bet a test exists specifically to let you stop making.

The trouble is that a useful test needs two things a lot of inherited code refuses to give it: a way to observe the result, and a way to control the inputs. Reading inherited code and reasoning your way to confidence is a different discipline from testing it, because reasoning is exactly what the original author also did, carefully, and it still produced the code you are now worried about changing. A test is a mechanical check that does not get tired, does not forget an edge case between Tuesday and the following sprint, and runs again automatically the next time anyone touches the file. Getting that mechanical check in place before a change, not after, has a name: Michael Feathers calls code with no tests covering it legacy code, independent of how old it is or how well it was written, precisely because the absence of a test is what makes a change risky, not the code's age.

The shipment-SLA checker worked through below has exactly the shape that makes this hard. It computes something that depends directly on the current moment, whether a shipment has missed its promised delivery window, and the inherited version answers that question by asking the operating system what time it is, right there inside the method that does the comparison. That is an ordinary way to write this kind of check, and it is also the one design decision that makes the check's exact boundary behavior impossible to pin down with a test until something changes about how the code is structured. The job ahead is not to rewrite the check. It is to make the check observable and controllable first, prove nothing moved while doing that, and only then touch the one behavior that was wrong.

{% include excalidraw.html name="27-seams" caption="Figure 27.1: the same inherited dependency made substitutable three ways, an object seam for a constructor-injected Clock, a link seam for a free function's grace-period policy, with the preprocessor seam held in reserve." %}

## Characterization tests: pinning what the code actually does

A characterization test is not an assertion about what a piece of code should do. It is a recorded observation of what it does do, written by running the code and copying down the answer, so that a later change to the code can be checked against that recorded answer instead of against memory or intuition. This distinction matters because inherited code frequently contains behavior nobody would sign off on if they were designing it today, an off-by-one at a boundary, a rounding rule that favors the wrong party, a silent fallback that masks an error. A characterization test pins that behavior exactly as found, wrong parts included, because the goal at this stage is not correctness. The goal is a safety net: a way to know, after any later edit, whether that edit changed behavior that was not supposed to change. You cannot decide whether a questionable behavior is a bug worth fixing until you can see it clearly and reproduce it on demand, and you cannot decide it is safe to fix until you have a test confirming every other behavior nearby stayed exactly where it was.

`LegacyShipmentSlaChecker` in this chapter's example is exactly this kind of inherited code. Its `is_breached()` method takes a shipment's service-level agreement, a tracking ID paired with a promised deadline, and answers one question: has this shipment missed that deadline.

```cpp
bool LegacyShipmentSlaChecker::is_breached(const ShipmentSla& sla) const {
    return std::chrono::system_clock::now() > sla.deadline;
}
```

Writing a characterization test for this method means constructing a `ShipmentSla` with a deadline, calling `is_breached()`, and recording what comes back. For a deadline well in the past, the answer is `true`. For a deadline well in the future, the answer is `false`. Both of those are straightforward to pin:

```cpp
TEST(LegacyShipmentSlaCheckerCharacterizationTest, DeadlineWellInThePastIsBreached) {
    LegacyShipmentSlaChecker checker;
    const ShipmentSla sla{"SHM-9001", std::chrono::system_clock::now() - std::chrono::hours(2)};

    EXPECT_TRUE(checker.is_breached(sla));
}
```

Notice the margin: two hours, not two seconds. That margin is not stylistic caution. It is a direct symptom of the hard dependency the next section removes. `is_breached()` calls `std::chrono::system_clock::now()` a second time, inside its own body, a few microseconds after this test computed its own `now()` to build the deadline. If the margin were small enough for those two readings of the clock to land close together, an unlucky scheduling delay between the two calls could flip the test's answer, and the test would be exercising operating-system jitter instead of the shipment checker's logic. Widening the margin to two hours makes that impossible, but it does not fix the underlying problem, it works around it.

The case this test file cannot write at all is a deadline exactly equal to the current moment. There is no way to make two separate calls to `std::chrono::system_clock::now()`, one inside the test and one inside `is_breached()`, land on the identical nanosecond. That is not a minor gap in test coverage. It is the concrete, measurable cost of a hard dependency: the checker's exact behavior at its own most important boundary, the instant a shipment's deadline arrives, cannot even be observed, let alone pinned, until something about the code changes. A characterization test suite that cannot reach a boundary condition is telling you precisely where the design needs a seam before it can tell you anything else.

## The object seam: making a dependency substitutable

Feathers' term for this kind of change point is a seam: a place in the code where you can alter behavior without editing the logic that behavior lives in. An object seam is the most common and most useful kind: give the class a way to receive its dependency from the outside, through a constructor parameter or a setter, behind a small interface, instead of reaching out and constructing or looking up that dependency itself. Once the dependency arrives from outside, whatever supplies it, production code with the real implementation, or a test with a substitute, decides what that dependency actually does on any given call.

`ShipmentSlaChecker` is `LegacyShipmentSlaChecker` refactored through exactly this seam. The wall-clock read moves out of the method body and becomes a `Clock` the constructor takes by reference:

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

`SystemClock`, the only concrete `Clock` the shipped binary ever constructs, does exactly what `LegacyShipmentSlaChecker` used to do inline: forward to the real system clock. Nothing about production behavior changes by introducing this interface; a `ShipmentSlaChecker` built with a `SystemClock` answers every question the legacy checker already answered, the same way, because it is reading the same clock through one additional virtual call. That claim is exactly what a refactor needs to prove before anyone trusts it, and it is provable directly: construct a `ShipmentSlaChecker` with a real `SystemClock`, run the same two-hour-margin scenarios the legacy characterization tests already pinned, and confirm the answers did not move.

```cpp
TEST(ShipmentSlaCheckerTest, SeamAloneDoesNotChangeTheWideMarginBehaviorTheLegacyCheckerHad) {
    SystemClock clock;
    ShipmentSlaChecker checker(clock);
    const ShipmentSla overdue{"SHM-9101", std::chrono::system_clock::now() - std::chrono::hours(2)};
    const ShipmentSla pending{"SHM-9102", std::chrono::system_clock::now() + std::chrono::hours(2)};

    EXPECT_TRUE(checker.is_breached(overdue));
    EXPECT_FALSE(checker.is_breached(pending));
}
```

That test passing is the entire argument that the seam itself is behavior-preserving. Nothing interesting has happened to the checker's answer yet. What has changed is what a test can now do to control it. A `MockClock`, a small GoogleMock subclass of `Clock`, lets a test dictate exactly what moment `now()` reports on each call, which turns the boundary that was unreachable a moment ago into three ordinary, instant, flake-proof tests: a deadline exactly equal to "now," one nanosecond before it, and one nanosecond after it.

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

No sleeping, no margin, no dependence on how fast the test machine happens to be running that day. `EXPECT_CALL` tells the mock exactly what to return the one time `is_breached()` asks it for the current moment, and the assertion checks the result against a deadline chosen by the test, down to the nanosecond. This is the payoff an object seam buys: not just the ability to avoid the real collaborator, but the ability to drive a specific, previously unreachable code path on command and check its exact result.

> **AI sidebar.** Ask an LLM pairing partner to "make this testable" and a common first move is reaching for a mocking framework's ability to intercept a free function or a static method, sometimes through a macro, sometimes through link-time trickery wrapped in a helper library, applied directly to the existing hard-coded call without touching the class's shape at all. That can work, and it avoids editing the class under test, which looks like the safer, smaller diff. Check what it actually bought you: if the interception only exists inside the test binary, nothing about the production code's design changed, the hidden dependency is still hidden, and the next person who wants to add a second implementation or reuse the checker somewhere else is back to square one. An object seam costs one constructor parameter and one small interface, visible in the production code, not just in test scaffolding, and it pays that cost back the first time any other caller needs a different collaborator too. Prefer the seam that changes the design over the one that only changes what the test binary can see.

## Changing behavior once the seam exists

The object seam does more than make the checker testable. It makes a deliberate behavior change safe. The legacy checker, reading `now() > deadline`, treats a deadline exactly equal to the current moment as not yet breached, an inherited quirk that nobody could even confirm with a test before this chapter's refactor, because the hard dependency made the exact boundary unreachable. Once `MockClock` can hit that instant on command, the question stops being hypothetical: a shipment's deadline arriving right now is the moment the service-level agreement is violated, and treating that instant as still-on-time was never a deliberate design choice. It was an artifact of a comparison operator nobody had looked at closely, because nobody could test it closely.

`ShipmentSlaChecker::is_breached()` makes the change directly: `clock_.get().now() >= sla.deadline`, strictly greater becomes greater-or-equal. What makes this change safe rather than reckless is doing it with the test suite watching the whole time: the wide-margin equivalence test above proves the far-from-the-boundary behavior never moved, and the exact-boundary tests pin the corrected rule precisely, each one asserting what the new comparison should produce for a deadline exactly at, one nanosecond before, and one nanosecond after the boundary. If a future edit ever reintroduces the old strict-inequality bug by accident, `DeadlineExactlyAtNowIsBreached` catches it immediately, because the test is checking the exact instant the bug would hide in, not a two-hour margin that would never notice.

The legacy checker's own comparison is never edited to match. `LegacyShipmentSlaChecker` stays in the codebase exactly as it was found, still answering `false` for the exact-boundary case, because it is the historical record the characterization tests pinned: a frozen reference for what the inherited code did before anyone changed anything, kept around precisely so a reviewer can compare the two side by side and see the one line that changed and nothing else.

## The link seam: a dependency with no object to inject into

An object seam needs an object: a class whose constructor a test can call with a different argument. Not every hard dependency in inherited code lives inside a class. `late_fee_cents()`, this chapter's second example, is a bare free function inherited from a billing batch job, called from many sites across that job, taking a count of minutes late and returning a fee in cents:

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

`grace_period()` is the hard dependency here, the enterprise policy for how many minutes of grace a shipment gets before a late fee starts accruing. There is no constructor anywhere near `late_fee_cents()` for a test to pass a different grace period into, because `late_fee_cents()` is not a method on anything. The obvious fix, giving `late_fee_cents()` a grace-period parameter, is also the correct long-term fix, and it is exactly the kind of change Feathers calls parameterizing a dependency once a direct edit is cheap enough to make. It stops being cheap the moment this function has forty call sites across a batch job you do not want to touch more than this one change requires; updating every call site today, in code you are not yet confident you understand, is a bigger and riskier edit than the test you are trying to add.

A link seam sidesteps that cost entirely by exploiting how a linker resolves a free function's definition. `grace_period()` is declared once:

```cpp
// grace_policy.hpp
std::chrono::minutes grace_period();
```

and defined twice, once in `grace_policy_production.cpp`, returning the real fifteen-minute enterprise policy, and once in `tests/grace_policy_test_double.cpp`, returning a deterministic zero minutes for tests. `late_fee_schedule.cpp`, which calls `grace_period()`, is compiled exactly once, into this example's `seam27_core` object library, and is linked into both the shipped demo binary and the test binary without ever being recompiled. Which definition of `grace_period()` a given binary ends up calling depends entirely on which `.cpp` file that binary's own CMake target compiles alongside the shared core: `seam27_demo` compiles the production file, `seam27_tests` compiles the test double. Neither the declaration in `grace_policy.hpp` nor a single line of `late_fee_schedule.cpp` changes between the two.

```cpp
TEST(LateFeeScheduleTest, FiveMinutesLateAccruesAFeeBecauseThisBinaryUsesTheZeroMinuteTestDouble) {
    EXPECT_EQ(late_fee_cents(5), 750);
}
```

That `750` is the proof the link seam worked, not an arbitrary assertion. Under the production fifteen-minute grace period, five minutes late never leaves the grace window, and `late_fee_cents(5)` would return zero. The nonzero answer is correct only because the test binary links the test double's zero-minute grace period instead; that correct answer, not just a passing compile, is what demonstrates the substitution took effect at link time. A link seam costs nothing in the production file and nothing in the function signature. It costs a second small translation unit and some care in the build configuration to make sure exactly one definition ends up in each binary, never both, which would be a duplicate-symbol link error the build would catch immediately rather than a silent mistake.

## The preprocessor seam: a last resort

Feathers describes a third seam for cases where neither of the first two is available: code compiled by a build system that cannot easily swap which translation unit gets linked, or a header-only function with no separate `.cpp` for a link seam to choose between. The preprocessor seam wraps the dependency in a conditional compilation directive, so the test build and the production build contain literally different compiled code at that one point:

```cpp
#ifdef SLA_DISABLE_PENALTY_LOGGING
inline void log_penalty_assessed(const ShipmentSla&) { /* no-op in the stripped test build */ }
#else
inline void log_penalty_assessed(const ShipmentSla& sla) { enterprise_audit_log::record(sla); }
#endif
```

Treat this as the seam you reach for only when the other two are truly unavailable, not as a convenient shortcut. A test that passes against code compiled with `SLA_DISABLE_PENALTY_LOGGING` defined proves something about that specific build configuration. It proves nothing about the configuration that actually ships, because the shipped binary runs different compiled instructions at that line, not the same instructions exercised differently. A bug that only manifests in the branch the macro strips out will never be caught by any test running against the macro-guarded build, no matter how thorough that test suite looks. The object seam and the link seam both preserve a single body of compiled logic and change only what it is handed from outside; the preprocessor seam gives up that guarantee in exchange for working anywhere, including places neither of the other two can reach. Use it there, and nowhere else, and treat every preprocessor-guarded branch as a standing reminder to come back and replace it with an object or link seam once the surrounding code is understood well enough to support one.

## The safety-net discipline, in order

The sequence this chapter has worked through is the discipline itself, and the order is not optional. Characterize first: observe and pin what the inherited code does right now, wide margins and unreachable boundaries included, before touching anything. Introduce a seam second: extract an interface, inject it through a constructor, or swap a linked translation unit, whichever fits the shape of the dependency, and prove with the existing characterization tests that the seam alone changed nothing. Change behavior third, and only third: once the seam exists and the pre-existing tests are green against it, make the deliberate change, add a test that pins the corrected behavior at the exact point that behavior lives, and watch every other test stay green while it does.

Skipping the first step means changing code you have not actually observed, trusting memory or a quick read instead of a recorded answer. Skipping the second step means changing behavior in code that still has no seam, which is the same bet this chapter opened with: confidence substituting for a mechanical check. Doing all three in order converts "I'm fairly sure this is still correct" into "the suite that passed before this change still passes after it, plus one new test for exactly what I meant to change." That is the entire value the safety net buys, and it buys nothing if any step is skipped or reordered.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/27-seams` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 11   # cpp23
100% tests passed, 0 tests failed out of 11   # cpp20
```

To see the legacy checker, the seamed checker, and the link-seamed late fee schedule run side by side at the terminal rather than read as assertions, run the example's own demo script:

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

Five minutes late produces no fee here because this is the demo binary, which links `grace_policy_production.cpp` and its real fifteen-minute grace period; `late_fee_schedule_test.cpp`'s assertion that five minutes late produces a fee is correct in the test binary precisely because the test binary links a different fifteen-minute-shorter policy instead. Running `examples/27-seams/demo.sh cpp20` prints identical output, because `ShipmentSlaChecker::try_time_remaining()` is the only piece of this example with a real C++20/C++23 difference, and the demo binary never calls it.

`try_time_remaining()` is worth a second look for exactly that difference. A caller that only needs the yes-or-no answer already has `is_breached()`; this method is for a caller, such as a billing job, that needs the magnitude too, how many minutes remain or how many minutes overdue a shipment is. C++20 can only report the boundary itself: `std::optional<std::chrono::minutes>` returns `nullopt` once the deadline has passed, discarding how far past it the checker actually is. C++23's `std::expected` keeps that overdue amount in its error channel instead of throwing it away, so a single call hands back both the yes/no answer and the magnitude:

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

Both versions compile against the identical `Clock` seam and are tested through the identical `MockClock`. Only the shape of the answer `try_time_remaining()` hands back when a shipment has already breached its deadline changed between standards, the same narrow scope this book's codetabs convention holds to everywhere else.

## When not to use it

A seam is not free. An object seam adds an interface and a virtual call where a concrete type and a direct call used to be, and a reader now has to open one more file to see which concrete `Clock` a given `ShipmentSlaChecker` is actually running against at any call site. A link seam adds a second translation unit and a build-configuration detail a future maintainer has to understand before they can safely add a third caller. Neither cost is large, but neither is zero, and paying it is only worth it when a real force justifies it.

Skip the seam entirely when the unit under suspicion is small enough, and understood well enough, to rewrite outright with a fresh, from-scratch implementation checked against a handful of example-based tests written against the *new* code's intended contract, not characterization tests pinning the old code's accidents. A ten-line helper function with one call site and an obvious intended behavior does not need the characterize-seam-change sequence; characterizing it, seaming it, and then changing it costs more than rewriting it correctly once and confirming the one call site still produces sensible results. The sequence pays off on code whose current behavior other systems depend on in ways nobody can fully enumerate from reading it, code with enough callers, enough history, or enough subtlety that an outright rewrite is a bet you cannot fully evaluate in advance. The two are easy to tell apart by asking one question: can you, right now, write down every behavior this unit needs to preserve, from memory, with confidence nobody will be surprised by an exception to your list next week? If yes, rewrite it and test the rewrite directly. If no, that uncertainty is exactly what a characterization test exists to resolve, and skipping straight to a rewrite is the same bet warned against above.

A seam is also the wrong tool when the "hard dependency" is not actually hard: a value computed once at program startup and never touched again has no real substitution need, and wrapping it behind an interface on principle is speculative generality wearing this chapter's vocabulary instead of solving the problem it exists to solve. Reach for an object seam, a link seam, or the preprocessor seam as a last resort when a test has no other way to observe or control a dependency, not as a default shape to apply to every inherited function on sight.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the original definition of a seam, the full catalogue of dependency-breaking techniques this chapter draws its object, link, and preprocessor seams from, and the fuller discussion of characterization tests as a discipline distinct from specification testing.
- Hyuk Myeong, *GoogleTest in Practice* (self-published): the GoogleMock idioms this chapter's `MockClock` and `EXPECT_CALL` usage follow, read alongside the official [GoogleTest documentation](https://google.github.io/googletest/).
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.
