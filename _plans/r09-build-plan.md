---
title: "r09 Build Plan"
output: false
---

# r09 — Part 9: Patterns in the wild

Third-to-last content iteration. Delivers Part 9 (chapters 24-26): the
patterns practitioners actually reach for under production constraints, the
lightweight shortcuts that stand in for a full GoF class hierarchy, and the
misuses to recognize and back out of. Verification is banked here, never on a
reader-facing page.

`part_name` for all three chapters: `patterns-in-the-wild` (see
`_parts/09-patterns-in-the-wild.md`, order 9).

## Chapters

| Ch | Title | Example dir | Target prefix |
|----|-------|-------------|---------------|
| 24 | Resource pools | `examples/24-resource-pools` | `pool24_` |
| 25 | Shortcut patterns: dispatch tables and maps | `examples/25-dispatch-tables` | `dispatch25_` |
| 26 | Anti-patterns and pattern misuse | `examples/26-anti-patterns` | `antipattern26_` |

### Ch 24 — Resource pools
Reusing expensive-to-create resources under a bound, and decoupling call from
execution. Covers: generic **resource/object pool** with an RAII lease handle
that returns the resource on destruction; **connection pool** as the canonical
instance (a bounded pool of shipment-tracking store connections); **thread
pool** of workers draining a task queue; **monitor object** (object owns its
mutex, methods serialized); **active object** (request queue + own thread,
returns `std::future`). Cross-link `[[project_cpp_concurrency]]` /
cpp-concurrency-and-parallelism for the deep threading mechanics — teach the
*pattern shape* here, not a concurrency primer. Codetabs where a real C++20/23
difference exists (`std::jthread`/`stop_token` C++20; `std::move_only_function`
and `std::expected` for `try_acquire` guarded for C++23).

### Ch 25 — Shortcut patterns: dispatch tables and maps
The lightweight alternatives to a full Strategy/Command/State hierarchy: an
enum-indexed **function-pointer table** (jump table) for shipment message-type
handlers; a `std::unordered_map<Key, handler>` **dispatch map** for
open/extensible registration; a `constexpr std::array` table for the closed
set. Contrast against the object-oriented forms from ch8/ch10/ch11 — when a
table is enough and when it is not. Codetabs: C++20 vs C++23 (`std::to_underlying`,
`std::flat_map` vs `std::map`). Cross-ref ch10 (FSM transition tables) and ch11
(Command).

### Ch 26 — Anti-patterns and pattern misuse
Recognizing patternitis: singleton-as-global-mutable-state, god object,
speculative generality, a Strategy/Factory with exactly one implementation,
abstraction that leaks. Example takes an over-patterned shipping design and its
simplified rewrite side by side, with tests that prove the behavior is
identical AND that the simplified version is testable where the original was
not (the singleton form cannot be isolated; the injected form can). Lighter on
codetabs — use them only where a genuine C++20/23 difference appears.

## Step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch release/r09 + write this plan | main | DONE |
| 1 | ch24 + examples/24-resource-pools + diagram (exec, standalone-verify both presets) | exec | TODO |
| 2 | ch25 + examples/25-dispatch-tables + diagram (exec, standalone-verify both presets) | exec | TODO |
| 3 | ch26 + examples/26-anti-patterns + diagram (exec, standalone-verify both presets) | exec | TODO |
| 4 | Register 3 diagrams in `_data/diagrams.yml` + write 3 `_examples/NN-*.md` cards | main | TODO |
| 5 | Integration `bundle exec jekyll build` (catches Liquid double-brace collisions) | main | TODO |
| 6 | Opus validation: superbuild both presets + voice + idiom gates | validate | TODO |
| 7 | Fix findings, re-verify | main | TODO |
| 8 | PR, squash-merge, tag r09.x, notes-only release | main | TODO |

## Acceptance criteria

1. Superbuild green on **both** presets (cpp23 and cpp20); every new test passes.
2. Each example is standalone-buildable (mirrored conanfile + presets, unique
   `*24_`/`*25_`/`*26_` target prefixes) and picked up by the root glob.
3. Each chapter has the full spine: learning objectives, a C4-style diagram (no
   UML), "why this matters" up top, "build, run, observe", "when not to use it",
   exactly one AI sidebar, "for deeper coverage".
4. 2,000-2,500 prose words per chapter; `lgtm-professional-voice scan.sh` = 0
   ban-tier, 0 em-dash; `lgtm-idiomatic-review` clean on each example.
5. Each chapter ships a paired `diagrams/*.svg` + `.excalidraw` (light-fill
   emphasis boxes) registered in `_data/diagrams.yml`; a gallery card in
   `_examples/`.
6. Jekyll build exits 0; adjacent opening braces in fenced code are wrapped in a
   Liquid raw block.
7. ch24 cross-links the concurrency book rather than duplicating threading
   mechanics.
8. **Low chapter-number cross-referencing.** Reference patterns/idioms BY NAME,
   not "as we saw in chapter 8" / "we cover this in chapter 11". Intra-book
   "see chapter N" plumbing reads poorly and breaks standalone reading; the
   voice reference books (Kubernetes Patterns, Geewax) do not use it. Genuine
   external pointers stay in "For deeper coverage". The validate/repair pass
   greps the three new chapters for chapter-number cross-refs and softens them.
   (Backlog beyond r09: sweep ch0-23 for the same.)
