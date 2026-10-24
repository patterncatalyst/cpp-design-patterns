---
title: "r02 Build Plan"
output: false
---

# r02 — Complete Parts 1 and 2

Second release iteration. Completes Part 1 (Recognizing design) and Part 2
(Principles / SOLID). Built via the relay (3 Sonnet executors, Opus validation).

## Scope and status

| Chapter / item | Status |
|---|---|
| ch2 Naming the pattern (AI/vocabulary) + diagram 02-shared-vocabulary | DONE (a12fbcb) — 2645 words |
| ch3 SOLID I: SRP/OCP/LSP + examples/03-solid-first-principles + diagram | DONE (d96beb8) — 17/15 tests |
| ch4 SOLID II: ISP/DIP + DI + examples/04-solid-dependency-inversion + diagram | DONE (fa28b74) — 14/14 tests |
| ch3 + ch4 /examples/ gallery cards | DONE (d96beb8) |
| bibliography.html GoogleTest-in-Practice fix (publisher/year/link) | DONE (23f5dfe) |
| Opus validation (all 9 criteria) | DONE — READY TO MERGE |
| ch4 prose fix ("constructs" not "names" concrete gateway) | DONE |

## Validation result (Opus, clean tree)

- Root superbuild, both presets: **cpp23 49/49**, **cpp20 45/45** (08-strategy 18/16, 03 17/15, 04 14/14). 03 and 04 standalone-buildable. No target collisions.
- Jekyll build exit 0; chapters 02/03/04 render; 3 example cards; 6 diagrams.
- Spine complete on all three; ch2 conceptual (no example, correct); ch3/ch4 have Build-run-observe + when-not + one AI sidebar + codetabs C++20/23.
- Voice 0 ban-tier, 0 em-dash; idiomatic review clean (integer money, OCP needs no edits, LSP avoids slicing via reference_wrapper, DIP concrete only in composition root, std::expected guarded).
- Demo output matches chapters byte-identical; ch3↔ch4 cross-reference resolves; part wiring correct.

## Deferred / follow-ups

- Example support files (README/CMakeLists/conanfile) carry em-dash + a few ban-tier phrases consistent across all examples; a one-pass voice sweep of example-support files is a future cleanup (not reader-site prose).
- "For deeper coverage" cites specific books + /bibliography/ but not the hub books.yml (book-wide convention from r01; revisit if desired).

## Next (r03)

Part 3 — The C++ substrate: ch5 RAII and deterministic lifetimes, ch6 value semantics/ownership/smart pointers, ch7 testing with GoogleTest.
