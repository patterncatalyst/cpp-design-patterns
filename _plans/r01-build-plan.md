---
title: "r01 Build Plan"
output: false
---

# Design Patterns with C++ — Project Plan and r01 Build Plan

Status board for the first release iteration. Verification status is banked here,
never on reader-facing pages.

## Project decisions (locked 2026-10-24)

- Repo `patterncatalyst/cpp-design-patterns`, PUBLIC, remote-first. First of a
  C++/Java/.NET/Python series (siblings later).
- All example code is NEW in our shipping-domain-code style (real domains, not
  placeholder classes). The archived `~/Dev/cpp-patterns/design-patterns-cpp` is
  reference-only for pattern/scenario selection, never copied.
- Standards shown side-by-side via codetabs: **C++20 / C++23 only**. C++26 is
  deferred (toolchain accepts `-std=c++2c` at flag level but the headline
  features are prerelease). Revisit later; hook documented in Appendix A.
- Diagrams: heavy, **no UML**, trending toward a C4-style standard. Paired
  SVG + `.excalidraw`; emphasis boxes light fill / dark text.
- No containers. Local tools only, with a prerequisites chapter.
- Everything tested with GoogleTest + CTest. Reference: *googletest in practice*
  (Hyuk Myeong) and the official GoogleTest docs.

## Verified toolchain (Fedora 44, 2026-10-24)

| Tool | Version | Notes |
|---|---|---|
| Clang | 22.1.8 | default compiler; C++23 + `std::print` + `std::expected` confirmed |
| GCC | 16.2.1 | supported alternate |
| CMake | 4.3.0 | floor `cmake_minimum_required(3.28)` |
| Ninja | 1.13.2 | default generator |
| Conan | 2.25.2 | manages all third-party deps |
| GoogleTest | 1.17.0 (via Conan) | `gtest_discover_tests` + CTest |

Presets: `cpp23` (default) and `cpp20` (compat). No `cpp26` preset yet.

## Chapter outline (33 chapters, 0-32, + Appendices A/B; 11 Arabic parts)

- Part 0 Orientation: 0 Prerequisites & the toolchain
- Part 1 Recognizing design: 1 Patterns as decisions under forces · 2 Naming the pattern (vocabulary for teams + AI)
- Part 2 Principles (SOLID, Martin): 3 SRP/OCP/LSP · 4 ISP/DIP + DI without a framework
- Part 3 The C++ substrate: 5 RAII & deterministic lifetimes · 6 Value semantics, ownership, smart pointers · 7 Proving it works: testing with GoogleTest
- Part 4 Selecting & varying behavior: 8 Strategy · 9 Template Method + NVI · 10 State & FSM incl. probabilistic · 11 Command, undo, command queues
- Part 5 Creating objects under control: 12 Factory Method + Abstract Factory · 13 Builder + Prototype (+serialization) · 14 Singleton & the DI alternative
- Part 6 Composing & adapting structure: 15 Adapter, Facade, Bridge (+PImpl) · 16 Decorator, Proxy · 17 Composite, Flyweight (+Null Object)
- Part 7 Coordinating & traversing: 18 Observer + Mediator · 19 Chain of Responsibility + Iterator (ranges) · 20 Visitor, Memento, Interpreter
- Part 8 Idiomatic C++ expressions: 21 CRTP + policy-based design · 22 Type erasure + PImpl · 23 Expression templates, serialization, prototype registries
- Part 9 Patterns in the wild: 24 Resource pools (connection/thread/object, monitor & active object) · 25 Shortcut patterns (fn-pointer tables & dispatch maps) · 26 Anti-patterns & pattern misuse
- Part 10 Changing inherited code (modernized Feathers): 27 Seams & the test safety net · 28 The 24 dependency-breaking techniques for C++20/23 · 29 Fowler vs Feathers contrasted + combined · 30 Refactor or rewrite
- Part 11 Putting it together: 31 Final project · 32 Working with AI & LLM tools
- Appendix A Build & toolchain reference (+ C++26 revisit hook) · Appendix B Pattern-to-force quick reference (completeness ledger)

All code new. Every pattern chapter ends with "when NOT to use it" + an AI sidebar.

## r01 step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Create public repo + branch release/r01 | main | DONE |
| 1 | Write this plan checkpoint | main | DONE |
| 2 | Scaffold Jekyll site (lgtm-jekyll): _config, layouts, includes incl. codetabs (C++20/23), index/chapters/examples/diagrams/bibliography, _parts stubs, CLAUDE.md | exec | DONE (114d0d0) |
| 3 | Build/test harness: root CMakeLists superbuild, conanfile.py (gtest 1.17.0), CMakePresets (cpp20/cpp23), scripts/ (check-prereqs, build, verify) | exec | DONE (114d0d0; +compile_commands 646a350) |
| 4 | CI: pages.yml + ci.yml (conan install + cpp23/cpp20 presets + ctest) | exec | DONE (114d0d0; dead voice step dropped 02164f2) |
| 5 | .gitattributes (Linguist → repo card reads C++) | exec | DONE (114d0d0) |
| 6 | examples/08-strategy: NEW domain example, three Strategy expressions, gtest, README build/run/observe, demo.sh | exec | DONE (3708a6c) — 18/16 tests pass |
| 7 | Chapter 0 Prerequisites | exec | DONE (2237b7f) |
| 8 | Chapter 1 Patterns as decisions under forces | exec | DONE (2237b7f) |
| 9 | Chapter 8 Strategy (codetabs C++20/23, when-not, AI sidebar, build/run/observe) | exec | DONE (e42cb41) |
| 10 | Diagrams: ch0 (build/test loop) + ch1 (the arc) + ch8 (strategy forces), paired SVG+excalidraw, no UML | exec | DONE (31bcdcf, af80bd5) |
| 10b | examples gallery card for 08-strategy | main | DONE (02164f2) |
| 11 | Validate r01 against acceptance criteria (voice + idiomatic-review gates) | validate | DONE — all 6 criteria MET, gates pass, verdict ready-to-merge |
| -- | Push release/r01, open PR #1, enable Pages | main | DONE (PR #1) |
| 12 | Hub PR: new "GoF Design Patterns and SOLID Principles" section + card + books | main | HOLD — needs user go (existing shared repo) |
| 13 | Squash-merge PR #1, tag r01.x, notes-only release | main | HOLD — needs user go (outward-facing) |
| 14 | Notion PRD rewrite (C++-only) + Java/.NET/Python sibling stubs | main | HOLD — needs user go (overwrites existing page) |

## r01 acceptance criteria

1. Public repo, Pages enabled, main protected; reached only via squash-merged PR; `r01.x` tag + notes-only release.
2. Clean checkout via prereqs page alone: `examples/08-strategy` builds under `--preset cpp23` and `cpp20`, `ctest` passes on clang 22.
3. `ci.yml` + `pages.yml` green; `jekyll build` exits 0; four collections resolve; `baseurl: /cpp-design-patterns`.
4. Live site renders ch0/ch1/ch8; codetabs switch C++20/23 and persist; ch8 has "Build, run, observe", "when NOT to use Strategy", AI sidebar; no verification footer on reader pages.
5. ch1 + ch8 are 2,000-2,500 prose words; `lgtm-professional-voice scan.sh` = 0 ban-tier; `lgtm-idiomatic-review` clean on the example.
6. Paired `diagrams/*.svg` + `.excalidraw` for ch1 + ch8; emphasis boxes light fill/dark text; no UML.
7. Hub builds green with the new section + card linking the live site; bibliography entries present.

## Risks

1. Scope sprawl → fixed 33+2 TOC, one canonical form per pattern, legacy capped at 4 chapters.
2. Toolchain friction (Conan/CMake/clang/gtest) → r01 proves the exact chain first; one dep manager; presets shared local + CI.
3. Outcome-framing bloat / drift from runnable code → pattern chapters gated on green ctest before prose counts; one AI sidebar box max.
4. Voice drift → rules in CLAUDE.md; voice gate every release PR.
