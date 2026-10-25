---
title: "r15 Build Plan"
output: false
---

# r15 — Testing-reference appendices

Three new reference appendices on the testing stack, plus a renumber. Reference
material: lighter spine (no runnable example, no AI sidebar, no 2,000-word floor),
voice-clean, publisher/official-doc links, no chapter-number prose plumbing (a
reference may link a chapter by name).

## Final appendix order (user-confirmed)
| Letter | Title | File | order |
|--------|-------|------|-------|
| A | Build and toolchain reference | 33-appendix-a-build-and-toolchain.md | 33 |
| B | Testing with GoogleTest | 34-appendix-b-testing-googletest.md (NEW) | 34 |
| C | Mocking with gMock | 35-appendix-c-mocking-with-gmock.md (NEW) | 35 |
| D | xUnit test patterns | 36-appendix-d-xunit-test-patterns.md (NEW) | 36 |
| E | Pattern-to-force quick reference | 37-appendix-e-pattern-to-force-reference.md (renamed from 34-appendix-b) | 37 |

## CRITICAL constraint (user)
ALL testing examples in B/C/D use the book's SHIPPING DOMAIN (shipments, carriers,
rates, pricing, SLA checkers, tracking-store connections, a Clock). NEVER the gMock
docs' MockTurtle/Painter example, and no UI-flavored or contrived samples. Mocks
are real collaborators (a TrackingStore, a CarrierGateway, a Clock). Executors
should look at the repo's real example types (examples/07-testing-with-gtest,
examples/24-resource-pools for the Clock seam, examples/26/31 for pricing) for
authentic names.

## Content
- **B Testing with GoogleTest**: TEST / TEST_F / TEST_P, the EXPECT_* vs ASSERT_*
  families and what nonfatal-vs-fatal means, common matchers, how the macros expand
  (what TEST(Suite,Name) actually generates), fixtures (SetUp/TearDown, per-test
  lifecycle), parameterized tests (INSTANTIATE_TEST_SUITE_P), typed/typed-param
  tests, death tests, and gtest_discover_tests + CTest. Source:
  https://google.github.io/googletest/primer.html + official docs + Googletest in
  Practice (self-published).
- **C Mocking with gMock**: MOCK_METHOD, setting expectations with EXPECT_CALL,
  matchers for arguments, cardinalities (Times), actions (WillOnce/WillRepeatedly/
  Return/Invoke), ordered expectations (InSequence), NiceMock/StrictMock/NaggyMock,
  and when a mock vs a fake is the right double. Source:
  https://google.github.io/googletest/gmock_for_dummies.html + gMock Cookbook +
  Googletest in Practice. RE-CAST every illustration as a shipping collaborator.
- **D xUnit test patterns**: Meszaros' four-phase test (setup/exercise/verify/
  teardown), the test-double taxonomy (dummy/stub/spy/mock/fake) mapped to how
  gMock/fakes express each, fixture strategies (fresh/shared/transient), assertion
  patterns, and the common test smells. Map each to GoogleTest/gMock and the book's
  example suites. Source: Gerard Meszaros, xUnit Test Patterns (Addison-Wesley,
  InformIT link in bibliography).

## Step board
| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch r15, rename E, Part-12 blurb, Meszaros in bibliography, plan | main | DONE |
| 1 | Appendix B (GoogleTest) | exec | TODO |
| 2 | Appendix C (gMock) | exec | TODO |
| 3 | Appendix D (xUnit patterns) | exec | TODO |
| 4 | Integration jekyll build; confirm A,B,C,D,E labels + Part 12 shows 5 cards | main | TODO |
| 5 | Opus validation: voice, links, shipping-domain (no MockTurtle), labels | validate | TODO |
| 6 | PR, squash-merge, tag r15.x, release | main | TODO |

## Acceptance criteria
1. Part 12 shows five cards labeled Appendix A-E in order; each appendix page's
   pill/num reads its letter; chapters 0-32 still read "Chapter N".
2. `bundle exec jekyll build` exits 0; B, C, D render.
3. Voice 0 ban-tier, 0 em-dash across B/C/D.
4. Every example in B/C/D is shipping-domain; ZERO occurrences of turtle/painter/
   MockTurtle or UI samples (validator greps).
5. Books/docs linked to publisher/official pages (GoogleTest docs, Meszaros ->
   InformIT); Googletest in Practice self-published, never Manning.
6. B/C/D have no "as we saw in chapter N" prose plumbing.
