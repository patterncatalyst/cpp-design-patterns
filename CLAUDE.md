# CLAUDE.md — Design Patterns with C++

Ground rules for working in this repo. Read `_plans/r01-build-plan.md` for
the current iteration's status board and acceptance criteria before
starting work; it is the source of truth for scope and is never touched by
content executors.

## Platform and tools

- Reference platform: **Fedora 44**. Local tools only — **no containers**
  for building or testing examples. CI uses a `fedora:44` job container
  purely as CI substrate (see `.github/workflows/ci.yml`'s header comment);
  that is not something a chapter ever tells a reader to do.
- Every example builds with **cmake + conan + clang** and is tested with
  the **GoogleTest suite, including GoogleMock**. (`ctest` is only CMake's
  runner for the gtest binaries, not a separate framework.) A test must
  verify *behavior* (the pattern does what it claims), not just that the
  binary exits 0.
- Pinned toolchain (see `_plans/r01-build-plan.md` for the verified
  versions): clang, CMake >= 3.28, Ninja, Conan 2.x, GoogleTest 1.17.0 via
  Conan. `scripts/check-prereqs.sh` verifies the floor versions;
  `scripts/build.sh [cpp23|cpp20]` builds; `scripts/verify.sh [cpp23|cpp20]`
  runs CTest.
- Every example under `examples/` is standalone-buildable on its own, in
  addition to being picked up by the root `CMakeLists.txt` superbuild.

## C++ standards

- This book covers exactly **C++20 and C++23**, side by side. Never write
  "C++21" — it does not exist as a standard name; the number after 20 is 23.
- **C++26 is deferred.** The toolchain accepts `-std=c++2c` at the flag
  level, but the headline features are prerelease. Do not add a `cpp26`
  preset, a third codetabs language, or C++26-only code anywhere. Revisit
  later; the hook for that is Appendix A (build & toolchain reference).
- Codetabs in chapter prose always use exactly:
  `{% raw %}{% include codetabs.html langs="C++20|C++23" %}{% endraw %}`
  — two labels, in that order, never more.

## Example code

- Every example is **new**, real-domain C++ written for this book, in the
  shipping-domain-code style: real nouns and scenarios (a reservation
  system, a telemetry pipeline, a billing engine), never `Foo`/`Bar`/`Baz`
  placeholder classes.
- The archived `~/Dev/cpp-patterns/design-patterns-cpp` repo is
  **reference-only** for pattern and scenario selection. Never copy code
  from it — not structure, not variable names, not comments.
- References for testing idiom: *GoogleTest in Practice* (Hyuk Myeong) and
  the official GoogleTest documentation. Use `gtest_discover_tests` so
  CTest sees every case.
- **Liquid gotcha:** Jekyll parses `{{ ... }}` as a Liquid tag even inside a
  fenced code block. If a C++ snippet contains adjacent braces (for example a
  map or nested initializer list like `{{a, b}, c}`), wrap that whole fenced
  block in `{% raw %}` / `{% endraw %}`, or the site build fails. Verify with a
  real `bundle exec jekyll build`, not just a standalone compile.

## Diagrams

- Diagrams are heavy — most chapters get at least one — and carry
  **no UML**: no class diagrams, no arrows-with-stereotypes notation.
  Trend toward a **C4-style** standard instead.
- Every diagram ships as a paired `diagrams/<name>.svg` +
  `diagrams/<name>.excalidraw`, generated with `lgtm-diagram-generator`.
- Emphasis/ink boxes are **light fill with dark text**, never a dark-filled
  box with light text.

## Chapter spine

Every chapter (`_docs/NN-*.md`) follows the same shape:

1. **Learning objectives** near the top.
2. **A diagram** (required for every new chapter).
3. **Why this matters**, near the top, before the mechanics.
4. The body.
5. **"For deeper coverage"** at the end, citing the hub's `books.yml` and
   this book's own `/bibliography/`.
6. **"Build, run, observe"** on every hands-on chapter, pointing at the
   matching `examples/` demo.

Every pattern chapter additionally ends with a **"when NOT to use it"**
section and carries exactly one **AI sidebar** box — no more than one per
chapter.

- Prose target: **2,000-2,500 words** of body prose (excluding code and
  diagrams). 2,000 is a hard floor; 2,500 is a target, not a ceiling — more
  is fine if it is value-driven (another diagram, another worked example),
  never padding for its own sake.
- Voice: lean on the *Kubernetes Patterns* and *API Design Patterns*
  (Geewax) register. No em-dash (zero tolerance), no rule-of-three, no
  contrived AI phrasing, no confessional register ("to be honest",
  "admittedly", "let's be real"). Run `lgtm-professional-voice` before
  calling a chapter done.
- Verification status (what's been run, what's pending) lives **only** in
  `_plans/`, never on a reader-facing page.
- Parts use **Arabic numbering** (Part 0, Part 1, ... Part 11) with a
  descriptive title, never Roman numerals.
- Companion decks are the **last** deliverable for a release iteration —
  written after every chapter and example for that iteration is done, not
  alongside them.

## Git workflow

- **Never commit directly to `main`.** Every change, including a one-line
  docs fix, goes branch → PR → squash-merge.
- Each release iteration gets its own `release/rNN` branch.

## Skills to use

- `lgtm-jekyll` — site scaffolding and diagram embedding conventions.
- `lgtm-tutorial` — chapter authoring workflow and per-chapter example
  scaffolding.
- `lgtm-presentation` — companion decks (last deliverable).
- `lgtm-diagram-generator` — paired SVG + `.excalidraw` figures.
- `lgtm-professional-voice` — voice pass before a chapter or release ships.
- `lgtm-idiomatic-review` — idiomatic-C++ review pass on example code.
- `lgtm-github` — branch/PR/tag/release workflow for this GitHub-hosted repo.
