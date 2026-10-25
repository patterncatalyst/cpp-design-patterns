---
title: "r17 build plan — Appendix F: Programming principles, reviewed in code"
order: 17
output: false
---

# r17 — Appendix F: Programming principles, reviewed in code

Post-completion iteration. Adds a sixth appendix (F) to the finished book: a
code-first review of the wider programming-principle vocabulary, recapping the
five SOLID principles (taught in full in the SOLID chapters) and adding the
broader catalogue. Inspired by the public programming-principles catalogue
(iluwatar, a fork of Lars Kappert's), but **not copied**: original prose and
original shipping-domain C++. The principle names and definitions are
common-knowledge industry concepts.

## Scope (user decisions, 2026-10-25)

- Full catalogue: a short code recap of the 5 SOLID (cross-linking the SOLID
  chapters, not duplicating them) PLUS the ~21 broader principles.
- Placement: new **Appendix F**, `_docs/38-appendix-f-programming-principles.md`
  (appendices are A=33 ... E=37, so F=38, `appendix: "F"`, `part: appendices`).
- Reference appendix, same class as D/E: inline before/after code snippets, NOT
  new runnable `examples/` modules. No CMake/superbuild changes.

## Principle families (section layout)

1. **Substitutability and responsibility (SOLID recap):** SRP, OCP, LSP, ISP,
   DIP — tight code recap, cross-link the SOLID chapters by name.
2. **Simplicity and scope:** KISS, YAGNI, Do The Simplest Thing That Could
   Possibly Work, Avoid Premature Optimization, Curly's Law (Do One Thing).
3. **Structure and change:** DRY, Separation of Concerns, Encapsulate What
   Changes, Hide Implementation Details, Composition Over Inheritance,
   Orthogonality.
4. **Coupling and cohesion:** Minimise Coupling, Maximise Cohesion, Law of
   Demeter, Command-Query Separation, Inversion of Control.
5. **Interoperability and resilience:** Robustness Principle (Postel's Law).
6. **Principles for teams and process (prose, no code):** Boy-Scout Rule, Code
   For The Maintainer, Murphy's Law, Brooks's Law, Linus's Law.

## Steps

1. Author `_docs/38-appendix-f-programming-principles.md` (Sonnet executor).
2. One C4-style diagram `38-principle-families` (SVG + .excalidraw) + register
   in `_data/diagrams.yml`; embed via the excalidraw include.
3. Update `_parts/12-appendices.md` blurb (five → six appendices).
4. Add only the newly cited books to `bibliography.html` with publisher links
   (The Pragmatic Programmer; Clean Code; The Mythical Man-Month). Never
   Amazon/Google links.
5. Opus validation pass (voice + idiomatic + build + criteria).
6. Ship: PR → squash-merge → tag `r17.x` → notes-only release.

## Acceptance criteria

- `_docs/38-appendix-f-programming-principles.md` renders as "Appendix F" (layout
  reads `appendix:` front-matter), listed under the Appendices part.
- All 26 principles present, grouped into the six families; every code-bearing
  principle has a short before (violates) / after (honors) shipping-domain C++
  snippet plus a one-line tension note; the five process "laws" are prose.
- SOLID recap cross-links the SOLID chapters by name, does NOT duplicate them,
  and uses NO chapter-number cross-references anywhere (name the principle/
  pattern instead).
- One paired C4-style diagram (light-fill emphasis boxes, no UML), registered
  and embedded with a `Figure` caption.
- "For deeper coverage" links cited books to their PUBLISHER pages; those books
  also appear in `/bibliography/`.
- `bundle exec jekyll build` exits 0 (watch the Liquid adjacent double-brace
  gotcha in any snippet with nested initializer braces).
- `lgtm-professional-voice` scan: 0 ban-tier (no em-dash, no rule-of-three, no
  confessional register, no editorial padding).
- No reader-facing verification footer; status lives here in `_plans/` only.

## Risks

- Breadth over depth: 26 principles risks a thin listicle. Mitigate by keeping
  each entry scannable but concrete (real before/after), not encyclopaedic.
- "Don't copy": do not reproduce the catalogue's wording or code. Original
  throughout; principle names are common knowledge.
- Overlap with SOLID chapters: recap only, cross-link by name, no duplication.
