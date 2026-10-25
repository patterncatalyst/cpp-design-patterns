---
title: "r12 Build Plan"
output: false
---

# r12 — Appendices (Part 12)

The two reference appendices. These are reference material, not tutorial
chapters: no runnable example, no AI sidebar, no 2,000-word floor. They must
still be voice-clean, link publisher/official-doc pages (never Amazon/Google),
and avoid chapter-number plumbing in prose (a reference ledger MAY link a pattern
to its chapter page by name as a navigation aid).

Wired as a new Part 12 (`_parts/12-appendices.md`, part_name `appendices`). Layouts
render "Appendix A/B" from a front-matter `appendix:` field instead of "Chapter N".

## Docs

| Doc | Title | order | appendix |
|-----|-------|-------|----------|
| A | Build and toolchain reference | 33 | "A" |
| B | Pattern-to-force quick reference | 34 | "B" |

### Appendix A — Build and toolchain reference (+ C++26 revisit hook)
One-stop reference for the build: reference platform (Fedora, local tools, no
containers), the pinned/floor toolchain table (clang >= 21, cmake >= 3.28, ninja,
conan 2.x, gtest 1.17.0 via Conan), `scripts/check-prereqs.sh`, the cpp23/cpp20
presets and what they set, Conan usage, per-example standalone build vs the root
superbuild glob, `gtest_discover_tests` + ctest, `compile_commands.json` for
editors (clangd note), and the **C++26 revisit hook**: the toolchain accepts
`-std=c++2c` but the headline features are prerelease; when revisiting, add a
cpp26 preset and a third codetab, and here is where each change goes.

### Appendix B — Pattern-to-force quick reference
A completeness ledger: every GoF pattern, SOLID principle, and C++ idiom in the
book mapped to the force(s) it answers and the chapter where it lives, each linked
to its chapter page by pattern name (navigation aid, not prose plumbing). Grouped
by family (principles, creational, structural, behavioral, C++ idioms, in-the-wild,
legacy). Short framing prose, then the tables.

## Step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch release/r12 + write this plan | main | DONE |
| 1 | Layout: render "Appendix X" from `appendix:` front matter (part_index + tutorial) | main | DONE |
| 2 | `_parts/12-appendices.md` | main | DONE |
| 3 | Appendix A doc | main | TODO |
| 4 | Appendix B doc | main | TODO |
| 5 | Integration `bundle exec jekyll build`; confirm Part 12 nav + "Appendix A/B" labels | main | TODO |
| 6 | Opus validation: voice + links + label rendering + ledger accuracy | validate | TODO |
| 7 | Fix findings; PR, squash-merge, tag r12.x, release | main | TODO |

## Acceptance criteria

1. Part 12 "Appendices" appears in nav with two cards labeled "Appendix A" and
   "Appendix B" (not "Chapter 33/34"); chapter pages for 0-32 still read "Chapter N".
2. `bundle exec jekyll build` exits 0; both appendix pages render.
3. Voice: 0 ban-tier, 0 em-dash across both appendices.
4. Links: any book cited links its publisher page (never Amazon/Google); official
   tool docs (CMake, Conan, GoogleTest) link their project sites; Googletest in
   Practice self-published, never "Manning".
5. No "as we saw in chapter N" prose plumbing; Appendix B's ledger links patterns
   to chapter pages by name.
6. Appendix A documents the real toolchain/presets/scripts (matches
   check-prereqs.sh, conanfile.py, CMakePresets.json) and carries the C++26 hook.
7. Appendix B covers all 23 GoF patterns + SOLID + the book's C++ idioms with no
   gaps (completeness ledger).
8. superbuild unaffected (no code touched) — spot-confirm still 475/436.
