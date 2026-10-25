---
title: "r10 Build Plan"
output: false
---

# r10 — Part 10: Changing inherited code (modernized Feathers)

The legacy-code track. Four chapters (27-30): get inherited code under test,
break its dependencies, combine Feathers with Fowler, and decide when to
refactor versus rewrite. This is the book's major methodology arc. Verification
is banked here, never on a reader-facing page.

`part_name` for all four chapters: `changing-inherited-code` (see
`_parts/10-changing-inherited-code.md`, order 10).

## Chapters

| Ch | Title | Example dir | Target prefix |
|----|-------|-------------|---------------|
| 27 | Seams and the test safety net | `examples/27-seams` | `seam27_` |
| 28 | The 24 dependency-breaking techniques for C++20/23 | `examples/28-dependency-breaking` | `breakdep28_` |
| 29 | Fowler and Feathers: contrasted and combined | `examples/29-refactoring-workflow` | `refactor29_` |
| 30 | Refactor or rewrite | `examples/30-refactor-or-rewrite` | `strangler30_` |

### Ch 27 — Seams and the test safety net
Characterization tests pin the current behavior of inherited code before any
change, and seams (object seam via dependency injection, link seam, and the
preprocessor seam as a last resort) are the places to alter behavior without
editing logic in place. Example: inherited shipping code with a hard dependency
(a wall-clock read or a global), characterization tests capturing its present
output, then an object seam introduced so the dependency can be substituted and
the behavior verified. Reference Feathers for the full technique.

### Ch 28 — The 24 dependency-breaking techniques for C++20/23
Feathers' 24 techniques, modernized. A reference table of all 24 with the C++20/23
tool that expresses each (concepts, `std::function`/`std::move_only_function`,
deducing-this, templates, link seams). Example demonstrates a representative
runnable subset (Extract Interface, Parameterize Constructor, Extract and
Override Call, Encapsulate Global References, Introduce Instance Delegator,
Adapt Parameter) on inherited shipping code, each with a test proving the
dependency is now breakable. Do not attempt all 24 in code; the table carries
the full catalogue, the example carries the representative subset.

### Ch 29 — Fowler and Feathers: contrasted and combined
REQUIRED: a markdown **contrast table** (Fowler's *Refactoring* vs Feathers'
*Working Effectively with Legacy Code*: starting assumption about tests, unit of
work, risk model, catalogue shape) AND a **"how they work together"** section.
The combined workflow: Feathers first to get code under characterization test and
break dependencies, then Fowler's named refactorings once a safety net exists.
Example: one inherited shipping component taken through the full sequence
(characterize, break dependency, then apply named Fowler refactorings), tests
green at every step.

### Ch 30 — Refactor or rewrite
The decision: when incremental refactoring wins and when a bounded rewrite does.
Strangler-fig replacement behind a stable interface, risk/cost/knowledge-in-the-
code factors. Example: a legacy pricing component replaced incrementally behind a
stable interface, tests proving behavior is preserved through the migration (old
and new implementations pass the same characterization suite).

## Step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch release/r10 + write this plan | main | DONE |
| 1 | ch27 + examples/27-seams + diagram (exec, standalone-verify both presets) | exec | TODO |
| 2 | ch28 + examples/28-dependency-breaking + diagram (exec, standalone-verify both presets) | exec | TODO |
| 3 | ch29 + examples/29-refactoring-workflow + diagram + contrast table (exec, standalone-verify) | exec | TODO |
| 4 | ch30 + examples/30-refactor-or-rewrite + diagram (exec, standalone-verify both presets) | exec | TODO |
| 5 | Register 4 diagrams in `_data/diagrams.yml` + write 4 `_examples/NN-*.md` cards | main | TODO |
| 6 | Integration `bundle exec jekyll build` | main | TODO |
| 7 | Opus validation: superbuild both presets + voice + idiom + cross-ref gates | validate | TODO |
| 8 | Fix findings, re-verify | main | TODO |
| 9 | PR, squash-merge, tag r10.x, notes-only release | main | TODO |

## Acceptance criteria

1. Superbuild green on both presets (cpp23 and cpp20); every new test passes.
2. Each example is standalone-buildable (mirrored conanfile + presets, unique
   `*27_`/`*28_`/`*29_`/`*30_` prefixes) and picked up by the root glob.
3. Each chapter has the full spine: learning objectives, a C4-style diagram (no
   UML), "why this matters" up top, "build, run, observe", "when not to use it"
   (or the equivalent judgment section), exactly one AI sidebar, "for deeper
   coverage" citing Feathers/Fowler/GoogleTest in Practice from `/bibliography/`.
3b. ch29 carries the required Fowler-vs-Feathers contrast table AND a
   "how they work together" section.
4. 2,000+ prose words per chapter (2,500 target, not a ceiling; no padding);
   `lgtm-professional-voice scan.sh` = 0 ban-tier, 0 em-dash; `lgtm-idiomatic-
   review` clean on each example.
5. Each chapter ships a paired `diagrams/*.svg` + `.excalidraw` (light-fill
   emphasis boxes) registered in `_data/diagrams.yml`; a gallery card in
   `_examples/`.
6. Jekyll build exits 0; adjacent opening braces in fenced code wrapped in a
   Liquid raw block.
7. **Low chapter-number cross-referencing (enforced from the start this time).**
   Reference patterns/idioms and the Feathers/Fowler techniques BY NAME, never
   "as we saw in chapter N" / "chapter N built". The validator greps all four
   chapters for chapter-number refs; executors are told the rule up front.
