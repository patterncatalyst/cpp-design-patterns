---
title: "r18 build plan — Appendix G: Large-scale C++ physical design"
order: 18
output: false
---

# r18 — Appendix G (C++-only, no sibling parity)

Post-completion addition (user request 2026-10-27): a seventh appendix, C++
specific, reviewing John Lakos's physical-design material in code. Source of
truth; never touched by content executors. Keep literal Liquid tags out of this
prose.

## Why this is C++-only
Physical design (the component, the include graph, levelization, insulation)
derives from C++'s separate compilation, header inclusion, the one-definition
rule, and the link model. Java, Python, and C# have module systems and a
link-free runtime, so the forces Lakos describes do not arise there. The
appendix states this up front; there is no Java/Python/.NET parity to build.

## Source book (add to bibliography)
John Lakos, *Large-Scale C++ Volume I: Process and Architecture*
(Addison-Wesley, 2019, ISBN 9780201717068). This is the "2nd edition", the
successor to the 1996 *Large-Scale C++ Software Design*. Volume II (Design and
Implementation) was not published, so cite Volume I precisely and do not
attribute unpublished Volume II implementation material to it. Publisher link:
InformIT product page for 9780201717068. Web-verify the title, ISBN, and link
at authoring time.

## Scope (ships) — FOCUSED (sections 1-4; packages/UOR are NOT a section)
- **Appendix G — "Large-scale C++: physical design, reviewed in code"**
  (`_docs/39-appendix-g-large-scale-cpp-physical-design.md`, order 39,
  `appendix: "G"`, `part: appendices`). Spine mirrors Appendix F
  (`_docs/38-appendix-f-programming-principles.md`): a short "How to read this
  appendix" intro, then concept sections, each code-bearing one with a short
  why, a before (violates) / after (honors) SHIPPING-DOMAIN C++20/23 snippet,
  and a one-line tension note.
  1. **The component** — the header/source pair as the atom of design, test,
     and reuse: logical and physical name cohesion, the rule that a source file
     includes its own header first (a self-sufficiency and include-what-you-use
     proof), one component is one primary class plus its tightly coupled
     helpers, and the component test driver (the `.t.cpp` idea). Before = a
     "god header" carrying many unrelated classes; after = factored components.
  2. **Physical dependencies and levelization** — the include graph as a
     directed acyclic graph, the ban on cyclic physical dependencies, level
     numbers, bottom-up testing, and 2-3 levelization techniques (escalation,
     demotion, opaque pointers, dumb data, the manager class) shown in code
     breaking a Shipment and Carrier cycle. Centerpiece diagram here.
  3. **Insulation versus encapsulation** — the precise distinction
     (encapsulation protects correctness from logical dependence; insulation
     protects clients from compile-time dependence). Total-insulation
     techniques reviewed in code: the PImpl / handle-body idiom, the protocol
     (pure-abstract) class, and the procedural interface. Tension: insulation
     costs an allocation or a virtual dispatch, so it earns its place across a
     stable release boundary, not everywhere. Reference the book's existing
     PImpl treatment BY NAME (it is taught as an idiom elsewhere in the book);
     here it is reframed as one insulation technique in Lakos's taxonomy. Do
     not re-teach it and do not cite a chapter number.
  4. **Reducing compile-time coupling (lightweight)** — forward declarations in
     place of includes, include-what-you-use, and transitive-include bloat.
     Tension: a forward declaration cannot back a value member or a base class.
  - Close with a short "when this is overkill" note that acknowledges the wider
    Lakos hierarchy (packages, package groups, units of release, three-letter
    package prefixes for link-time name cohesion) WITHOUT a dedicated section:
    a small codebase takes the component and levelization discipline and skips
    the package-group ceremony.
- Two C4-style diagrams (paired SVG + .excalidraw, light fill, no UML),
  generated with lgtm-diagram-generator:
  - `39-levelization-before-after` — a cyclic include graph beside a levelized
    directed acyclic graph with level numbers. Embedded as Figure G.1.
  - `39-insulation-firewall` — the compile-time firewall: a client depends on a
    stable interface while the implementation stays hidden behind it. Embedded
    as Figure G.2.
- Update `_parts/12-appendices.md` blurb: six -> seven appendices (add the
  physical-design clause).
- Add Lakos Volume I to `bibliography.html` with the InformIT publisher link.

## Steps
1. Executor (Sonnet): author Appendix G + the two diagrams + the bibliography
   row + the part blurb. Reference the two embedded figures the way Appendix F
   references its figure (an excalidraw include with a Figure caption).
2. Orchestrator: register `39-levelization-before-after` and
   `39-insulation-firewall` in `_data/diagrams.yml`; integration build. No new
   example modules, so the superbuild and CTest counts are unchanged; still run
   a build to confirm nothing broke, plus `bundle exec jekyll build`.
3. Opus validation with a HARD ACCURACY GATE -> fix findings -> PR ->
   squash-merge -> tag `r18.x` -> release.
4. Hub re-sync: run the hub `gen-books.py` (its HTML-table parser now reads
   `bibliography.html`), confirm Lakos lands in `_data/books.yml` with
   cpp-design-patterns in referenced_by, and categorize it in
   `book_categories.yml`. Separate hub PR.

## Acceptance criteria
- Renders as "Appendix G" under the Appendices part (7 appendices A-G).
  Sections 1-4 present; each code-bearing concept has a before/after
  shipping-domain C++20/23 snippet and a tension note. No dedicated
  packages/UOR section; the closer acknowledges the hierarchy briefly.
- Lakos's terminology is used EXACTLY as he defines it: the
  encapsulation-versus-insulation distinction, the levelization technique
  names, and the component rules must match the source. This is the primary
  validation target.
- Two paired C4 diagrams, light fill, no UML, registered and embedded with
  Figure G.1 / G.2 captions.
- Idiomatic C++20/23 in the snippets; shipping-domain nouns only
  (Shipment/Carrier/RateQuote/TrackingRecord/Invoice), never Foo/Bar.
- PImpl is referenced by name as an insulation technique, not re-taught; NO
  intra-book chapter-number cross-references anywhere in the prose.
- "For deeper coverage" cites Lakos Volume I with the PUBLISHER link; the book
  is in `/bibliography/`.
- 0 ban-tier voice; no em-dash. `bundle exec jekyll build` exit 0; superbuild
  and CTest unchanged (no new example modules).

## Notes
- Orchestrator owns `_data/diagrams.yml`, `_plans/`, `_parts/`, git, and the
  hub re-sync.
