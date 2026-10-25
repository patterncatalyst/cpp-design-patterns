---
title: "r11 Build Plan"
output: false
---

# r11 — Part 11: Putting it together

The closing content part. Two chapters (31-32): a final project that composes
several patterns from the book into one coherent system, and a chapter on working
with AI and LLM tools as a pattern-literate collaborator. After r11: Appendices
A/B, then the companion decks (the final deliverable). Verification is banked
here, never on a reader-facing page.

`part_name` for both chapters: `putting-it-together` (see
`_parts/11-putting-it-together.md`, order 11).

## Chapters

| Ch | Title | Example dir | Target prefix |
|----|-------|-------------|---------------|
| 31 | Final project: composing patterns into a system | `examples/31-final-project` | `finalproject31_` |
| 32 | Working with AI and LLM tools | `examples/32-ai-and-llm-tools` | `aicollab32_` |

NOTE: the term is **"final project"**, never "capstone".

### Ch 31 — Final project
A larger worked example that composes several patterns the book already built
into one coherent shipping system, showing how they interact rather than
re-teaching each. Compose at least: Strategy (rate selection), Factory (carrier
creation), State / finite-state machine (shipment lifecycle), Command with undo
(operations), and Observer (lifecycle-event notifications). Wire them into one
runnable application with integration tests that prove the patterns work together
(a shipment moves through its lifecycle, fires events, computes a rate, and an
operation is undone). The chapter walks the composition and the seams between
patterns; it does not re-explain each pattern from scratch.

### Ch 32 — Working with AI and LLM tools
Using LLMs as a pattern-literate collaborator, not a code vending machine. The
engineer recognizes the forces and decides; AI accelerates. Cover: prompting with
the forces vocabulary and pattern names; the failure modes (AI over-patterns a
3-line problem, hallucinates APIs, writes non-idiomatic or outdated-standard
C++); and the verification discipline that makes AI output safe (gate every
AI-suggested pattern on behavior tests and an idiomatic-review pass; use AI for
the legacy work too, characterization tests and seam-finding). Example: a runnable
demonstration of the verify-AI-output discipline, e.g. an AI-plausible but subtly
wrong pattern implementation beside a correct one, with a characterization/
behavior test suite that catches the difference, proving why AI output is gated on
tests rather than trusted on read.

## Step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch release/r11 + write this plan | main | DONE |
| 1 | ch31 + examples/31-final-project + diagram (exec, standalone-verify both presets) | exec | TODO |
| 2 | ch32 + examples/32-ai-and-llm-tools + diagram (exec, standalone-verify both presets) | exec | TODO |
| 3 | Register 2 diagrams in `_data/diagrams.yml` + write 2 `_examples/NN-*.md` cards | main | TODO |
| 4 | Apply publisher links to both chapters' For-deeper-coverage (orchestrator owns URLs) | main | TODO |
| 5 | Integration `bundle exec jekyll build` | main | TODO |
| 6 | Opus validation: superbuild both presets + voice + idiom + cross-ref + link gates | validate | TODO |
| 7 | Fix findings, re-verify | main | TODO |
| 8 | PR, squash-merge, tag r11.x, notes-only release | main | TODO |

## Acceptance criteria

1. Superbuild green on both presets (cpp23 and cpp20); every new test passes.
2. Each example standalone-buildable (mirrored conanfile + presets, unique
   `finalproject31_`/`aicollab32_` prefixes) and picked up by the root glob.
3. Each chapter has the full spine: learning objectives, a C4-style diagram (no
   UML), "why this matters" up top, "build, run, observe", "when not to use it"
   (or judgment equivalent), exactly one AI sidebar, "for deeper coverage".
4. 2,000+ prose words per chapter (2,500 target, not a ceiling; no padding);
   voice 0 ban-tier, 0 em-dash; idiomatic-review clean on each example.
5. Paired `diagrams/*.svg` + `.excalidraw` (light-fill emphasis boxes) registered
   in `_data/diagrams.yml`; gallery card in `_examples/`.
6. Jekyll build exits 0; adjacent opening braces in fenced code wrapped in a
   Liquid raw block.
7. **Zero intra-book chapter-number cross-references.** Reference patterns/idioms
   BY NAME (ch31 especially, since it composes earlier patterns). Executors told
   up front; validator greps.
8. **Publisher links in "For deeper coverage"** (publisher page, never
   Amazon/Google); *Googletest in Practice* = self-published, no purchase link,
   official GoogleTest docs link; never "Manning". Orchestrator applies URLs.
9. Term is **"final project"**, never "capstone" (grep both chapters).
