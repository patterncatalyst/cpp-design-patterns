// deck-testing-legacy-ai.js - "Design Patterns with C++: Testing, Legacy
// Code & AI" - a standalone companion deck covering testing with
// GoogleTest/gMock, changing inherited code, working with AI/LLM tools,
// and the final project that composes five patterns into one system.
//
// Build:  node deck-testing-legacy-ai.js
// Paths:  deck-helpers.js reads DECK_ASSETS (./assets) and DECK_PNG (./png).

"use strict";

const H = require("./deck-helpers.js");
const {
  COLOR, FONT, W, ASSETS,
  newDeck, addFooter, addContentTitle, addBullets, addTwoColBullets,
  addStatusTable, addCaption, addCodeSlide, addDiagramSlide, addSectionDivider, addNotes,
} = H;

const OUT = "cpp-design-patterns-testing-legacy-ai.pptx";
const REV = "v1.0";

const pres = newDeck();
pres.title = "Design Patterns with C++: Testing, Legacy Code & AI";
let pageNum = 0;

function S() {
  const s = pres.addSlide(); pageNum += 1; addFooter(s, pageNum); return s;
}
function divider(code, title, subtitle, notes) {
  const s = pres.addSlide(); pageNum += 1;
  addSectionDivider(s, code, title, subtitle);
  addNotes(s, notes);
  return s;
}

// Dense reference table (local to this deck only): smaller fonts than
// addStatusTable so a 12-row catalogue fits one slide without the table
// overflowing past the footer. Same 3-column (code|name|purpose) shape.
function addDenseRefTable(slide, rows, opts = {}) {
  const y = opts.y ?? 1.75;
  const colW = opts.colW ?? [3.40, 4.30, 4.39];
  const rowH = opts.rowH ?? 0.33;
  const tableRows = rows.map((r) => [
    { text: r.code, options: {
        bold: true, color: r.codeColor || COLOR.red,
        fontFace: FONT.mono, fontSize: 11, align: "left", valign: "middle",
    }},
    { text: r.name, options: {
        bold: true, color: COLOR.ink,
        fontFace: FONT.body, fontSize: 11, align: "left", valign: "middle",
    }},
    { text: r.purpose, options: {
        color: COLOR.body,
        fontFace: FONT.body, fontSize: 10, align: "left", valign: "middle",
    }},
  ]);
  slide.addTable(tableRows, {
    x: 0.62, y, w: 12.09,
    fontFace: FONT.body, color: COLOR.body,
    border: { type: "solid", color: COLOR.grid, pt: 0.5 },
    valign: "middle",
    colW, rowH,
  });
}

// =============================================================================
// Cover
// =============================================================================
{
  const s = pres.addSlide();
  pageNum += 1;
  s.background = { color: COLOR.white };
  try { s.addImage({ path: `${ASSETS}/cover-panel.png`, x: 0, y: 0, w: W, h: 7.5 }); } catch (e) {}
  s.addText("DESIGN PATTERNS WITH C++", {
    x: 6.00, y: 1.98, w: 6.90, h: 0.34,
    fontFace: FONT.title, fontSize: 14, bold: true, color: COLOR.red,
    charSpacing: 6, align: "left", valign: "middle",
  });
  s.addText("Design Patterns\nwith C++", {
    x: 5.95, y: 2.42, w: 6.95, h: 1.90,
    fontFace: FONT.title, fontSize: 48, bold: true, color: COLOR.ink,
    align: "left", valign: "top",
  });
  s.addText("Testing, Legacy Code & AI", {
    x: 6.00, y: 4.55, w: 6.70, h: 0.60,
    fontFace: FONT.body, fontSize: 20, italic: true, color: COLOR.caption,
    align: "left", valign: "top",
  });
  s.addText(REV, {
    x: 11.85, y: 5.85, w: 0.95, h: 0.30,
    fontFace: FONT.mono, fontSize: 11, color: COLOR.caption, align: "right", valign: "middle",
  });
  try { s.addImage({ path: `${ASSETS}/logo-candidate-2.png`, x: 11.10, y: 6.80, w: 1.55, h: 0.37 }); } catch (e) {}
  addNotes(s,
    "This deck is for engineers who already know the pattern catalogue and now need to put it into " +
    "practice: proving a pattern works with a real test suite, changing code they inherited without " +
    "breaking it, and working alongside an AI pairing partner without losing the judgment calls that " +
    "stay with the engineer. Every example here uses a shipping domain: a quote intake, a tracking " +
    "store, a carrier gateway, a shipment dispatcher. Nothing in this deck is a toy UI sample. The arc " +
    "runs from testing, through changing inherited code, through working with AI tooling, to a final " +
    "project that composes five patterns into one running system.");
}

// =============================================================================
// Agenda
// =============================================================================
{
  const s = S();
  addContentTitle(s, "AGENDA", "What this deck covers");
  addTwoColBullets(s, [
    "Testing the work: GoogleTest styles, gMock interaction tests, and the xUnit test-double vocabulary",
    "Changing inherited code: seams, the dependency-breaking catalogue, Fowler and Feathers combined, refactor or rewrite",
  ], [
    "Working with AI and LLM tools: prompting with forces, the failure modes, the verification gate",
    "Putting it together: five patterns composed into one shipment dispatch system",
    { text: "Appendix: a test-double taxonomy and the dependency-breaking catalogue, both as lookup tables", muted: true },
  ], { fontSize: 17 });
  addNotes(s,
    "Four working sections plus a reference appendix. Testing and changing inherited code are the two " +
    "biggest sections because they are the two skills a pattern catalogue alone never teaches: proving a " +
    "pattern's claim with a real suite, and getting existing code safe to touch before changing it. " +
    "Working with AI and LLM tools treats a model as a fast collaborator whose output still has to pass " +
    "the same gates as anyone else's code. Putting it together is the integrative close, one dispatcher " +
    "composing Strategy, Factory, State, Command, and Observer. The appendix is pure lookup: a taxonomy " +
    "table and the full dependency-breaking catalogue, for after the talk.");
}

// =============================================================================
// SECTION 1 - Testing the work
// =============================================================================
divider("01", "Testing the work", "GoogleTest, gMock, and the xUnit vocabulary behind every pattern's proof",
  "This section is the shared testing foundation the rest of the deck assumes: the GoogleTest styles " +
  "that prove a pattern does what it claims, gMock for verifying an interaction rather than just a " +
  "return value, and the xUnit vocabulary, the four-phase test and the test-double taxonomy, that gives " +
  "a team shared words for what a test double is actually doing. Every example from here on uses a real " +
  "shipping collaborator: a quote ledger, a tracking store, a carrier gateway, an injectable clock. None " +
  "of it is a placeholder class. By the end of this section a pattern chapter's test suite should read as " +
  "proof of a specific claim, not boilerplate around an assertion.");

{
  const s = S();
  addContentTitle(s, "TESTING THE WORK · GOOGLETEST", "TEST, TEST_F, TEST_P, and when a failure should stop");
  addTwoColBullets(s, [
    "TEST registers a plain case; a file-scope static object adds it to the test registry before main() runs",
    "TEST_F derives from a fixture class, so SetUp()/TearDown() wrap the test body automatically",
    "TEST_P derives from TestWithParam<T>; GetParam() reads whichever row the suite is currently feeding it",
  ], [
    "EXPECT_* records a failure and keeps running; two independent checks both get to report",
    "ASSERT_* records a failure and returns immediately; reach for it before a std::optional or pointer dereference",
    "A streamed message, ASSERT_TRUE(x) << \"...\", prints only on failure and should name what probably broke",
  ], { fontSize: 16 });
  addNotes(s,
    "All three macros produce a registered class; they differ only in what that class inherits and how " +
    "many instances end up in the registry. The EXPECT versus ASSERT choice is a design decision, not a " +
    "style preference: dereferencing a std::optional after a failed has_value() check is undefined " +
    "behavior, so that check has to be ASSERT_TRUE, while two independent facts about the same result, a " +
    "price and an id, both deserve to run and both deserve to report even if only one is wrong. A " +
    "regression that breaks one while leaving the other correct should show up as exactly one failing " +
    "line, not a test that stops after the first problem and hides the second.");
}

{
  const s = S();
  addDiagramSlide(s, "TESTING THE WORK · GOOGLETEST", "Five styles, one discovery flow", "07-test-styles",
    "Five test styles, each answering a different question, funnel into one CTest run.");
  addNotes(s,
    "A fixture, a parameterized suite, a typed suite, a mock-interaction test, and a death test each " +
    "answer a different question about the same component, a freight carrier's quote intake. None of " +
    "them duplicate each other's job: a fixture shares setup across many behaviors, a parameterized test " +
    "proves one behavior across many input values, a typed test proves one contract across many concrete " +
    "types, a mock test verifies an interaction instead of a return value, and a death test proves a " +
    "precondition is fatal rather than recoverable. All five register with GoogleTest's discovery " +
    "mechanism the same way, so CTest reports every case individually, not pass-or-fail per binary.");
}

{
  const s = S();
  addCodeSlide(s, "TESTING THE WORK · GOOGLETEST", "A table of rules replaces five near-identical tests", "C++ · gtest",
    [
      "struct ValidationCase {",
      "    std::string name;",
      "    ShipmentRequest request;",
      "    ValidationOutcome expected;",
      "};",
      "",
      "class ShipmentValidationTest",
      "    : public ::testing::TestWithParam<ValidationCase> {};",
      "",
      "TEST_P(ShipmentValidationTest, MatchesExpectedOutcome) {",
      "    const auto& test_case = GetParam();",
      "    EXPECT_EQ(validate_shipment(test_case.request), test_case.expected)",
      "        << \"case \\\"\" << test_case.name << \"\\\" did not resolve\";",
      "}",
    ],
    "A name-generator lambda on INSTANTIATE_TEST_SUITE_P makes a failing case legible by name alone.");
  addNotes(s,
    "validate_shipment() checks three independent rules: weight must be positive, the destination zone " +
    "must resolve, and the declared value must sit under an insurable limit. Writing one TEST per rule " +
    "scales badly, a fourth rule means a fourth copy-pasted function. TEST_P plus " +
    "INSTANTIATE_TEST_SUITE_P collapses that into one assertion and a table of named cases. Without a " +
    "name-generator lambda, GoogleTest numbers each instance by index, and a failure points at a number a " +
    "reader has to go count rows to identify. With it, ctest -N prints the case name directly, so a " +
    "sixth intake rule later means adding a row, not a new function and a new chance at a copy-paste bug.");
}

{
  const s = S();
  addCodeSlide(s, "TESTING THE WORK · GOOGLETEST", "A fixture resets shared state in both directions", "C++ · gtest",
    [
      "class QuoteIntakeTest : public ::testing::Test {",
      "protected:",
      "    void SetUp() override {",
      "        SequentialQuoteIdGenerator::reset_sequence_for_test(1);",
      "    }",
      "    void TearDown() override {",
      "        SequentialQuoteIdGenerator::reset_sequence_for_test(1);",
      "    }",
      "",
      "    MockQuoteLedger ledger_;",
      "    SequentialQuoteIdGenerator id_generator_;",
      "    QuoteIntake intake_{ledger_, id_generator_};",
      "};",
    ],
    "Resetting in TearDown, not only SetUp, protects the next test even when this one fails fatally.");
  addNotes(s,
    "Every test in this suite needs a ledger, an id generator, and a QuoteIntake wired to both; a " +
    "fixture builds them once as protected members instead of duplicating four lines across nine tests. " +
    "SetUp and TearDown matter here specifically because the id generator's counter is a static member " +
    "standing in for a database sequence shared across every QuoteIntake in the process, not something a " +
    "fresh fixture instance resets on its own. TearDown resets it too, not only SetUp, because a fatal " +
    "ASSERT_* failure partway through a test body skips straight to TearDown. Cleanup written only at " +
    "the bottom of a test function would silently never run on exactly the run where something broke.");
}

{
  const s = S();
  addContentTitle(s, "TESTING THE WORK · GOOGLETEST", "Typed tests: one contract, more than one concrete type");
  addBullets(s, [
    "TYPED_TEST_SUITE proves one contract holds across more than one concrete type with no shared base class",
    "The fixture is a class template, instantiated once per entry in a ::testing::Types list",
    "Every TYPED_TEST body runs against every instantiation; a third generator needs one new line, no new test code",
    "Reach for a typed test when the shape is one contract and many types; reach for TEST_P when it's one type and many input values",
  ], { fontSize: 17 });
  addNotes(s,
    "A sequential quote-id generator and a prefixed one share no base class at all, only a method name " +
    "and an unwritten promise: next_id() never returns an empty string and never repeats within the " +
    "process. A typed test proves that promise once, against a ::testing::Types list, instead of writing " +
    "the same check out by hand for every concrete generator. The two shapes are easy to confuse: a " +
    "parameterized test varies the input to one type under test, a typed test varies the type under test " +
    "against one fixed check. Reaching for a typed test when only one concrete implementation exists and " +
    "no second one is coming is premature machinery for a contract nothing else needs to honor yet.");
}

{
  const s = S();
  addContentTitle(s, "TESTING THE WORK · GMOCK", "MOCK_METHOD and EXPECT_CALL: the gMock anatomy");
  addBullets(s, [
    "MOCK_METHOD(ReturnType, name, (Args...), (specifiers...)) replaces a virtual method with a mock; the parens matter even for zero arguments",
    "EXPECT_CALL is the assertion and the stand-in behavior together, verified when the mock object is destroyed, not when the line runs",
    "ON_CALL sets a default answer with no claim on whether or how often the call happens; an EXPECT_CALL's action always wins over it",
    "A mock must override every pure virtual method its interface declares, or it stays abstract and a test can't construct it at all",
  ], { fontSize: 17 });
  addNotes(s,
    "The specifier list is where const, noexcept, and override go, each its own entry inside its own " +
    "parentheses; without the parens the macro can't tell where the argument list ends and the specifier " +
    "list begins, since a bare comma-separated list looks identical either way to the preprocessor. " +
    "EXPECT_CALL sets an expectation before the code under test runs, and a test that sets one, never " +
    "triggers it, and falls off the end fails with a message naming the exact unsatisfied line. ON_CALL " +
    "is for a supporting collaborator, a clock wired in purely so a timestamp field isn't left " +
    "default-constructed, while the real EXPECT_CALL carries the assertion the test is actually about.");
}

{
  const s = S();
  addCodeSlide(s, "TESTING THE WORK · GMOCK", "A tracking-store mock: both methods, mocked", "C++ · gmock",
    [
      "class TrackingStore {",
      "public:",
      "    virtual ~TrackingStore() = default;",
      "    virtual void record_event(const std::string& id,",
      "                               TrackingEvent event) = 0;",
      "    virtual std::optional<TrackingEvent>",
      "        latest_event(const std::string& id) const = 0;",
      "};",
      "",
      "class MockTrackingStore : public TrackingStore {",
      "public:",
      "    MOCK_METHOD(void, record_event,",
      "        (const std::string& id, TrackingEvent event), (override));",
      "    MOCK_METHOD(std::optional<TrackingEvent>, latest_event,",
      "        (const std::string& id), (const, override));",
      "};",
    ],
    "A mock implements every pure virtual method its interface declares, with no exceptions.");
  addNotes(s,
    "This is a shipment-tracking store that a label-printing workflow writes to and reads back from. " +
    "Notice the specifier lists: record_event's is (override) alone, latest_event's is (const, " +
    "override), because the real interface marks that lookup const. A mock that leaves even a " +
    "non-mocked pure virtual unimplemented is still an abstract class, so it won't link into a test " +
    "binary at all, whether or not any test ever calls that method. Non-pure virtual methods are " +
    "different: leave one unmocked and the mock simply inherits the base class's implementation for it, " +
    "which is sometimes exactly what a test wants.");
}

{
  const s = S();
  addCodeSlide(s, "TESTING THE WORK · GMOCK", "A carrier-gateway mock with an argument matcher", "C++ · gmock",
    [
      "class MockCarrierGateway : public CarrierGateway {",
      "public:",
      "    MOCK_METHOD(CarrierQuote, request_quote,",
      "        (const ShipmentRequest& request), (const, override));",
      "};",
      "",
      "using ::testing::AllOf;",
      "using ::testing::Field;",
      "using ::testing::Gt;",
      "",
      "EXPECT_CALL(gateway,",
      "    request_quote(AllOf(Field(&ShipmentRequest::weight_kg, Gt(0.0)),",
      "                         Field(&ShipmentRequest::zone,",
      "                               ServiceZone::Regional))))",
      "    .WillOnce(Return(CarrierQuote{\"GRD-1121\", 6'200}));",
    ],
    "Field reaches into the argument itself; a whole-struct Eq would miss the wrong zone.");
  addNotes(s,
    "A bare literal inside EXPECT_CALL's parentheses, like request_quote(_), is treated as Eq() " +
    "automatically. Field and AllOf matter when the bug to catch is a call that passes the right weight " +
    "but the wrong destination zone, which a whole-struct equality check would never distinguish from a " +
    "correct call. Field reaches into one public data member and matches on it independently; AllOf " +
    "combines two Field matchers so the expectation only fires on a request that satisfies both. This is " +
    "the matcher vocabulary that turns 'was this method called' into 'was this method called with " +
    "exactly the argument the test cares about,' which is almost always the more useful question.");
}

{
  const s = S();
  addCodeSlide(s, "TESTING THE WORK · GMOCK", "An injectable clock turns an unreachable boundary testable", "C++ · gmock",
    [
      "class Clock {",
      "public:",
      "    virtual ~Clock() = default;",
      "    virtual std::chrono::system_clock::time_point now() const = 0;",
      "};",
      "",
      "class MockClock : public Clock {",
      "public:",
      "    MOCK_METHOD(std::chrono::system_clock::time_point, now,",
      "        (), (const, override));",
      "};",
      "",
      "TEST(ShipmentSlaCheckerTest, DeadlineExactlyAtNowIsBreached) {",
      "    MockClock clock;",
      "    EXPECT_CALL(clock, now()).WillOnce(Return(deadline));",
      "    ShipmentSlaChecker checker(clock);",
      "    EXPECT_TRUE(checker.is_breached({\"SHM-9103\", deadline}));",
      "}",
    ],
    "No sleeping, no margin: MockClock hits the exact boundary a wall-clock read never could.");
  addNotes(s,
    "A shipment-SLA checker that calls the system clock directly can only ever be tested with a wide " +
    "margin, hours rather than seconds, because two separate reads of the real clock cannot land on the " +
    "identical nanosecond. Injecting a Clock interface moves the read out from inline and lets a " +
    "MockClock dictate exactly what instant now() reports. That turns a deadline exactly equal to 'now,' " +
    "previously unobservable, into an ordinary, instant, flake-proof test. This is the same object-seam " +
    "idea the next section builds out in full against inherited code; here it shows up first as an " +
    "ordinary testing tool, before it becomes a legacy-code technique.");
}

{
  const s = S();
  addStatusTable(s, [
    { code: "Times(n)", name: "Exactly n calls", purpose: "States the call count outright as the assertion" },
    { code: "AtLeast/AtMost(n)", name: "A bound, not exact", purpose: "n or more calls, or up to n calls" },
    { code: "Return(value)", name: "Canned answer", purpose: "Returns value from the matched call" },
    { code: "WillOnce / WillRepeatedly", name: "Chained actions", purpose: "One action per call, then a repeating default once the chain runs out" },
    { code: "Invoke(callable)", name: "Computed answer", purpose: "Forwards the call's arguments to callable for a non-canned result" },
  ], { colW: [2.60, 2.90, 6.59] });
  addContentTitle(s, "TESTING THE WORK · GMOCK", "Cardinalities and actions, at a glance");
  addNotes(s,
    "Times(0) is the cardinality that turns a non-interaction into the assertion itself: a rejected " +
    "shipment must never reach the ledger at all, and Times(0) on record() is exactly that claim. When " +
    "Times is omitted, gMock infers a count from the attached actions, one WillOnce with no " +
    "WillRepeatedly implies exactly one call, a trailing WillRepeatedly implies at least as many calls as " +
    "the WillOnce chain. WillOnce is consumed after it fires once; a second matching call falls through " +
    "to the next WillOnce or to WillRepeatedly. Invoke is the escape hatch for behavior too specific for " +
    "a canned Return, such as a quote computed from the actual request instead of a fixed number.");
}

{
  const s = S();
  addStatusTable(s, [
    { code: "Plain mock", name: "Naggy by default", purpose: "Logs a warning to the test log on an unexpected call" },
    { code: "NiceMock<T>", name: "Stays silent", purpose: "Quiets a collaborator that only plays a supporting role" },
    { code: "StrictMock<T>", name: "Fails the test", purpose: "Use when a code path must never call this collaborator at all" },
  ], { colW: [2.60, 2.90, 6.59] });
  addContentTitle(s, "TESTING THE WORK · GMOCK", "The nice/strict spectrum");
  addNotes(s,
    "An unexpected call, one that matches no EXPECT_CALL, is still answered with a default value so the " +
    "test keeps running; the three wrappers disagree on what else happens. A plain mock is called naggy " +
    "for a reason: every call a test never bothered to expect prints a warning, useful while a test is " +
    "being written and noisy once a suite has dozens of mocks with incidental calls. NiceMock quiets a " +
    "Clock a pricing test wires in purely to avoid a default-constructed timestamp. StrictMock goes the " +
    "other way: wrap a tracking store in StrictMock when the whole point of a test is that a code path " +
    "must never write to it, so any other call fails loudly instead of going unnoticed.");
}

{
  const s = S();
  addContentTitle(s, "TESTING THE WORK · XUNIT PATTERNS", "The four-phase test, and when to reach for a fake");
  addTwoColBullets(s, [
    "Setup (arrange): build the object under test and its collaborators",
    "Exercise (act): call the one behavior under test",
    "Verify (assert): check the outcome",
    "Teardown: release what setup acquired, reset anything global",
  ], [
    "A mock is right when the next sentence names a method and an argument",
    "A fake is right when the next sentence names a value the collaborator should be holding afterward",
    "An in-memory TrackingStore fake beats a mock once logic has to behave consistently across several calls in one test",
  ], { fontSize: 16 });
  addNotes(s,
    "Every test splits into these four phases whether the author labels them or not. A GoogleTest " +
    "fixture splits setup and teardown out of the test body entirely, so a TEST_F body holds only " +
    "exercise and verify; setting an EXPECT_CALL is still setup, not verify, even though it reads like an " +
    "assertion, because it declares what the mock should do before the call happens. The mock-versus-fake " +
    "line is the one engineers blur most: writing a connection pool's test double as a mock means " +
    "chaining a WillOnce for every connection the test ever acquires, turning a two-line fake into a " +
    "maintenance burden that only grows.");
}

{
  const s = S();
  addStatusTable(s, [
    { code: "Fragile test", name: "Breaks on a behavior-preserving refactor", purpose: "Depend on the interface, never a concrete implementation's internals" },
    { code: "Erratic test", name: "Passes or fails by run order or wall clock", purpose: "Inject a Clock; reset process-wide state in SetUp and TearDown" },
    { code: "Obscure test", name: "A failure doesn't say what broke", purpose: "One behavior per TEST_F, named for that behavior" },
    { code: "Test duplication", name: "The same wiring retyped every test", purpose: "Fixture members plus a custom assertion helper" },
    { code: "Conditional logic", name: "A test branches or loops over cases", purpose: "TEST_P with a table, not a loop that can skip a case" },
    { code: "Slow test", name: "Milliseconds of logic, seconds of runtime", purpose: "A fake in-memory store stands in for a real connection" },
  ], { colW: [2.70, 4.10, 5.29], rowH: 0.42 });
  addContentTitle(s, "TESTING THE WORK · XUNIT PATTERNS", "Six test smells, and the fix this book's examples apply");
  addNotes(s,
    "Meszaros catalogs dozens of smells; these six show up most often in a C++ suite built on GoogleTest " +
    "and gMock. The erratic-test fix is the same object seam the injectable-clock slide showed earlier: " +
    "replace a direct system_clock::now() call with an injected Clock double, and reset any process-wide " +
    "state such as a sequential id generator's counter in both SetUp and TearDown, since a fatal assertion " +
    "mid-test jumps straight to TearDown. The obscure-test fix is just naming discipline: " +
    "RejectsAnInvalidShipmentWithoutTouchingTheLedger tells a reader what broke before they open the file; " +
    "Test3 tells them nothing.");
}

// =============================================================================
// SECTION 2 - Changing inherited code
// =============================================================================
divider("02", "Changing inherited code", "Seams, the dependency-breaking catalogue, and the call on refactor versus rewrite",
  "Inherited code was written to satisfy whatever requirement was in front of its author at the time, " +
  "under whatever pressure was in front of them. Nobody asked whether a future maintainer would need to " +
  "substitute one of its collaborators for a test double, because the change that will need the " +
  "substitution hadn't been imagined yet. This section works through the discipline that makes changing " +
  "that code safe: characterize first, open a seam second, change behavior third, with a test suite " +
  "watching the whole time. It also covers the full catalogue of techniques for opening a seam, when " +
  "Fowler's refactoring catalogue and Feathers' seam-finding catalogue each apply, and how to decide " +
  "between refactoring in place and a bounded, strangler-fig rewrite.");

{
  const s = S();
  addContentTitle(s, "CHANGING INHERITED CODE", "Characterization tests: pinning what the code actually does");
  addBullets(s, [
    "A characterization test records what inherited code actually does right now, wrong parts included, not what it should do",
    "The goal is a safety net: a way to tell, after a later edit, whether behavior that wasn't supposed to change actually stayed put",
    "A wide test margin, two hours rather than two seconds, is a symptom of a hard dependency, not a stylistic choice",
    "A boundary a characterization test can't even reach is telling you exactly where a seam has to go before anything else is testable",
  ], { fontSize: 17 });
  addNotes(s,
    "Michael Feathers calls code with no tests covering it legacy code, independent of how old or how " +
    "well written it is, precisely because the absence of a test is what makes a change risky. A " +
    "characterization test for a shipment-SLA checker that reads the wall clock directly can only pin a " +
    "deadline well in the past or well in the future; it cannot pin a deadline exactly equal to the " +
    "current moment, because no two calls to the real clock can land on the identical nanosecond. That " +
    "is not a minor coverage gap. It is the concrete, measurable cost of the hard dependency, and it " +
    "names exactly where a seam has to go before the next step can even be attempted.");
}

{
  const s = S();
  addCodeSlide(s, "CHANGING INHERITED CODE · CHARACTERIZATION", "A wide margin hides a hard dependency", "C++ · gtest",
    [
      "bool LegacyShipmentSlaChecker::is_breached(",
      "        const ShipmentSla& sla) const {",
      "    return std::chrono::system_clock::now() > sla.deadline;",
      "}",
      "",
      "TEST(SlaCheckerCharacterizationTest,",
      "     DeadlineWellInThePastIsBreached) {",
      "    LegacyShipmentSlaChecker checker;",
      "    const ShipmentSla sla{\"SHM-9001\",",
      "        std::chrono::system_clock::now() - std::chrono::hours(2)};",
      "    EXPECT_TRUE(checker.is_breached(sla));",
      "}",
    ],
    "A two-hour margin is a workaround, not a fix: it hides the hard dependency, it doesn't remove it.");
  addNotes(s,
    "This passes today and it will keep passing after any later change that preserves the legacy " +
    "behavior, which is exactly what a characterization test is for. The margin is not stylistic " +
    "caution: is_breached() calls system_clock::now() a second time inside its own body, microseconds " +
    "after the test computed its own 'now' to build the deadline, and a small enough margin risks an " +
    "unlucky scheduling delay flipping the answer. Widening the margin to two hours makes that " +
    "impossible, but it works around the problem rather than fixing it. The case this test file cannot " +
    "write at all, a deadline exactly equal to the current moment, is the next slide's subject.");
}

{
  const s = S();
  addDiagramSlide(s, "CHANGING INHERITED CODE · SEAMS", "Three ways to make a dependency substitutable", "27-seams",
    "One hard-wired clock, shown three ways: no seam, an object seam, a link seam.");
  addNotes(s,
    "A seam, in Feathers' vocabulary, is a place where behavior can change without editing the logic " +
    "that behavior lives in. An object seam gives a class a way to receive its dependency from outside, " +
    "through a constructor, behind a small interface. A link seam exploits how a linker resolves a free " +
    "function's definition, declaring it once and defining it twice, once for production and once for a " +
    "test binary, so no object exists to inject anything into. A preprocessor seam, held in reserve, " +
    "wraps the dependency in a conditional compilation directive and is the last resort when neither of " +
    "the other two is available, because it means the test build and the shipped build run different " +
    "compiled instructions at that one point.");
}

{
  const s = S();
  addCodeSlide(s, "CHANGING INHERITED CODE · OBJECT SEAM", "The seam alone proves itself behavior-preserving", "C++ · gtest",
    [
      "class ShipmentSlaChecker {",
      "public:",
      "    explicit ShipmentSlaChecker(const Clock& clock)",
      "        : clock_(clock) {}",
      "    bool is_breached(const ShipmentSla& sla) const {",
      "        return clock_.get().now() >= sla.deadline;",
      "    }",
      "private:",
      "    std::reference_wrapper<const Clock> clock_;",
      "};",
      "",
      "TEST(ShipmentSlaCheckerTest, DeadlineExactlyAtNowIsBreached) {",
      "    MockClock clock;",
      "    EXPECT_CALL(clock, now()).WillOnce(Return(deadline));",
      "    ShipmentSlaChecker checker(clock);",
      "    EXPECT_TRUE(checker.is_breached({\"SHM-9103\", deadline}));",
      "}",
    ],
    "The comparison also changed, > to >=; the wide-margin tests still pass because far-from-boundary behavior never moved.");
  addNotes(s,
    "Introducing the Clock seam changes nothing by itself: construct the checker with a real SystemClock, " +
    "re-run the legacy checker's wide-margin tests against it, and the answers don't move. That equivalence " +
    "test is the entire proof the seam is behavior-preserving. Only once MockClock can hit the exact " +
    "boundary on command does the deliberate change become safe: is_breached() moves from strict " +
    "greater-than to greater-or-equal, because a deadline arriving right now is the moment an SLA is " +
    "violated. The legacy checker is never edited to match; it stays in the codebase as the frozen record " +
    "of what inherited code did before anyone touched it, for a reviewer to compare line by line.");
}

{
  const s = S();
  addContentTitle(s, "CHANGING INHERITED CODE · LINK SEAM", "A dependency with no object to inject into");
  addBullets(s, [
    "late_fee_cents() is a bare free function with no constructor nearby for a test to pass a different grace period into",
    "A link seam declares the dependency once and defines it twice: a production .cpp and a test-double .cpp, never both in one binary",
    "Which definition a binary calls depends on which .cpp its own build target compiles alongside the shared core, not on any source change",
    "Reach for a link seam when parameterizing the function outright means touching call sites you don't yet have confidence to change",
  ], { fontSize: 17 });
  addNotes(s,
    "grace_period() is the hard dependency here, the enterprise policy for how many minutes of grace a " +
    "shipment gets before a late fee starts accruing, called from many sites across a billing batch job. " +
    "The obvious fix, giving late_fee_cents() a grace-period parameter, is the correct long-term fix and " +
    "stops being cheap the moment forty call sites in code you don't yet understand well would all need " +
    "to change in the same edit. A link seam sidesteps that cost entirely: neither the header declaration " +
    "nor the calling function's source changes at all. The seam lives entirely in which .cpp file each " +
    "binary's own build target happens to compile alongside the shared logic.");
}

{
  const s = S();
  addCodeSlide(s, "CHANGING INHERITED CODE · LINK SEAM", "One declaration, two definitions, two binaries", "C++ · CMake",
    [
      "// grace_policy.hpp: declared once",
      "std::chrono::minutes grace_period();",
      "",
      "// grace_policy_production.cpp: linked into the shipped binary",
      "std::chrono::minutes grace_period() {",
      "    return std::chrono::minutes{15};",
      "}",
      "",
      "// tests/grace_policy_test_double.cpp: linked into the test binary",
      "std::chrono::minutes grace_period() {",
      "    return std::chrono::minutes{0};",
      "}",
      "",
      "TEST(LateFeeScheduleTest, FiveMinutesLateAccruesAFeeHere) {",
      "    EXPECT_EQ(late_fee_cents(5), 750);",
      "}",
    ],
    "750 cents proves the substitution took effect at link time: production's grace period would return 0.");
  addNotes(s,
    "Under the production fifteen-minute grace period, five minutes late never leaves the grace window " +
    "and late_fee_cents(5) would return zero. The nonzero 750 is correct only because the test binary " +
    "links the zero-minute test double instead of the production definition, which is the proof the link " +
    "seam worked, not an arbitrary assertion. A link seam costs nothing in the production file and " +
    "nothing in the function's signature; it costs a second small translation unit and some build-" +
    "configuration care to make sure exactly one definition lands in each binary, never both, which " +
    "would otherwise be a duplicate-symbol link error the build catches immediately.");
}

{
  const s = S();
  addDiagramSlide(s, "CHANGING INHERITED CODE · DEPENDENCY BREAKING", "Opening a mouth for a seam", "28-dependency-breaking",
    "ShipmentAlertNotifier moves from a hard-wired sender to an injected AlertChannel.");
  addNotes(s,
    "Extract Interface pulls AlertChannel's one method, send(), out of SmtpAlertSender's full surface; " +
    "extracting the interface alone changes nothing about how the notifier gets its dependency, it only " +
    "creates the possibility of a second implementation. Parameterize Constructor is the move that opens " +
    "the seam, and the detail that makes it safe for inherited code is the default argument: a new " +
    "constructor parameter defaults to building the exact same concrete sender the old constructor used " +
    "to build internally, so every existing call site still compiles and still behaves identically. Only " +
    "a test supplies the second argument, a mock or a recording fake, with no socket ever opened.");
}

{
  const s = S();
  addTwoColBullets(s, [
    "Extract Interface pulls a collaborator's minimal contract into an abstract base so a double can implement just that",
    "Parameterize Constructor adds a defaulted collaborator parameter; every existing call site keeps compiling unchanged",
    "Extract and Override Call isolates one troublesome expression: a C++20 virtual override or a C++23 deducing-this hook",
  ], [
    "Encapsulate Global References wraps a mutable global behind an interface without moving the global itself",
    "Introduce Instance Delegator puts an injectable object in front of a static-only API with zero existing callers touched",
    "Adapt Parameter shrinks a heavyweight parameter down to the handful of values a method actually reads",
  ], { fontSize: 16 });
  addContentTitle(s, "CHANGING INHERITED CODE · DEPENDENCY BREAKING", "Six techniques from the full 24-entry catalogue");
  addNotes(s,
    "Feathers' catalogue stays narrow by design: a team facing a global variable and a team facing a " +
    "static-only utility class are facing structurally different problems, and reaching for 'add " +
    "dependency injection' as a blanket answer to both usually means a much larger rewrite than either " +
    "problem actually requires. Extract and Override Call is the one entry C++20 and C++23 express two " +
    "different ways: a protected virtual method a test subclass overrides at runtime, or a deducing-this " +
    "hook resolved at compile time with no vtable at all. The full catalogue of 24, mapped to a modern " +
    "C++20/23 tool for each one, is in the appendix.");
}

{
  const s = S();
  addDiagramSlide(s, "CHANGING INHERITED CODE · DEPENDENCY BREAKING", "A seam in front of a static-only API", "28-instance-delegator",
    "A delegator seams a static lookup; the legacy call site never changes.");
  addNotes(s,
    "LegacyCarrierZoneLookup is a static-only utility called directly from many places; changing its " +
    "signature or replacing it with an instance-based design outright would mean touching every one of " +
    "those callers in the same change, a scope no single test-writing task should have to accept. " +
    "Introduce Instance Delegator solves this without touching the legacy type at all: a thin instance " +
    "class implements a new interface and forwards every call straight to the static method. New code is " +
    "written against the interface from the start; the delegator adds nothing to the lookup logic " +
    "itself, its entire job is being an object a caller can hold and a test can replace, which a bare " +
    "static method never can.");
}

{
  const s = S();
  addCodeSlide(s, "CHANGING INHERITED CODE · DEPENDENCY BREAKING", "Introduce Instance Delegator", "C++ · header",
    [
      "class ZoneLookupPort {",
      "public:",
      "    virtual ~ZoneLookupPort() = default;",
      "    virtual int zone_for(const std::string& postal_code) const = 0;",
      "};",
      "",
      "class CarrierZoneLookupDelegator : public ZoneLookupPort {",
      "public:",
      "    int zone_for(const std::string& postal_code) const override {",
      "        return LegacyCarrierZoneLookup::zone_for(postal_code);",
      "    }",
      "};",
    ],
    "New code depends on ZoneLookupPort; the legacy static call site keeps calling the static method directly.");
  addNotes(s,
    "A ShipmentRoutingAdvisor written from the start against ZoneLookupPort gets driven through a " +
    "GoogleMock MockZoneLookupPort with no legacy table involved at all, while an existing call site " +
    "elsewhere in the codebase keeps calling LegacyCarrierZoneLookup::zone_for() directly and compiles " +
    "unchanged. The delegator doesn't improve the zone-boundary logic inside the legacy lookup; it is " +
    "exactly as opaque after this change as before it. What changed is that new code now has something " +
    "to hold a reference to and a test can substitute, which is the entire and only point of this " +
    "technique: it gets code under test, it does not improve its design.");
}

{
  const s = S();
  addDiagramSlide(s, "CHANGING INHERITED CODE · FOWLER AND FEATHERS", "Two books, one shared target", "29-fowler-and-feathers",
    "Feathers builds the safety net first; Fowler's refactorings reshape the code after.");
  addNotes(s,
    "Fowler's Refactoring opens from a position of trust: a fast test suite already exists, and the " +
    "book's job is teaching a catalogue of small, named, behavior-preserving moves that reshape a code's " +
    "internal structure. Feathers' Working Effectively with Legacy Code opens from the opposite position: " +
    "most inherited code has no such suite, usually for a structural reason, a hard-wired collaborator or " +
    "a long parameter list. Applying a Fowler-style refactoring to code with no tests isn't risky because " +
    "the transformation itself is dangerous in isolation; it's wrong because nothing will tell you if you " +
    "got it wrong. The two books are sequential stages of one job, not competing schools.");
}

{
  const s = S();
  addStatusTable(s, [
    { code: "Starting assumption", name: "A fast, trustworthy suite already exists", purpose: "No such suite exists, and the code's structure is usually why" },
    { code: "Unit of work", name: "A named refactoring applied under the suite", purpose: "A seam found, a dependency broken, a characterization test written" },
    { code: "Risk model", name: "The suite catches a mistake within seconds", purpose: "Change as little as possible to open the first seam" },
    { code: "Catalogue shape", name: "About ninety named refactorings", purpose: "Twenty-four dependency-breaking techniques" },
    { code: "Right lens when", name: "Coverage exists; the question is how to shape it", purpose: "No coverage exists; the question is what it does today" },
  ], { colW: [2.40, 4.70, 4.99], rowH: 0.57 });
  addContentTitle(s, "CHANGING INHERITED CODE · FOWLER AND FEATHERS", "Contrasted: Fowler versus Feathers");
  addNotes(s,
    "The risk-model row is the one that trips engineers who have only read one of the two books. " +
    "Fowler's refactorings are safe specifically because a test suite is there to catch a mistake within " +
    "seconds, and that safety is borrowed from an asset Feathers' techniques exist to create in the first " +
    "place. A codebase with full coverage and a tangled if/else chain needs Fowler's catalogue and " +
    "nothing from Feathers, there's no dependency to break. A fifteen-year-old module with a direct clock " +
    "read buried in a two-hundred-line method needs Feathers first, because no named refactoring is safe " +
    "to apply to code you cannot run a regression test against.");
}

{
  const s = S();
  addContentTitle(s, "CHANGING INHERITED CODE · FOWLER AND FEATHERS", "The combined workflow, in order");
  addBullets(s, [
    "Characterize first: record what's already observable, because 'unchanged' is the property every later step must preserve",
    "Break the one dependency blocking the rest: Extract Interface plus Parameterize Constructor opens the seam",
    "Extend the characterization test to pin what was previously unobservable, now against a fixed substituted value",
    "Only then open the refactoring catalogue: Extract Function, Replace Conditional with Polymorphism, Introduce Parameter Object, suite green after each step",
  ], { fontSize: 17 });
  addNotes(s,
    "This order cannot run backward for a structural reason, not a procedural one: a named refactoring " +
    "is only as safe as the suite that verifies it, and a suite cannot verify behavior it has no way to " +
    "observe. Applying Replace Conditional with Polymorphism to a carrier-dispatch chain that reads a " +
    "live system clock inside the same method means every test run risks landing on a different side of " +
    "a seasonal boundary, making a failing test indistinguishable from one that was always going to fail " +
    "depending on the calendar. The dependency has to be broken first, specifically because breaking it " +
    "is what turns 'I think this still works' into 'the suite says this still works.'");
}

{
  const s = S();
  addDiagramSlide(s, "CHANGING INHERITED CODE · REFACTOR OR REWRITE", "A strangler fig behind a stable interface", "30-refactor-or-rewrite",
    "A strangler fig router shifts traffic from the legacy engine to the modern one.");
  addNotes(s,
    "Every inherited codebase eventually produces a component nobody wants to touch directly: a pricing " +
    "engine, a tax calculator, a fraud scorer. The strangler fig, named for a tree that envelops its " +
    "host until the host can be removed, replaces that component behind a stable interface " +
    "incrementally. Callers depend only on the interface; a router decides, per request, whether the " +
    "legacy implementation, the new one, or both should handle it; and a shared characterization suite " +
    "decides whether the new implementation has earned the right to take traffic. The old implementation's " +
    "share doesn't drop to zero in one release, it shrinks customer by customer, each increment observable " +
    "and reversible.");
}

{
  const s = S();
  addTwoColBullets(s, [
    "Risk: what breaks, and who notices first",
    "Cost: a long calendar spread versus a concentrated block with dual maintenance",
    "Undocumented knowledge: a zone-rate matrix built from years of lane negotiations is the business logic, not noise to clean up",
  ], [
    "How well the current code is understood today",
    "Whether a safety net already exists",
    "The cost of being wrong: wasted time versus a half-built replacement and a frozen legacy system",
  ], { fontSize: 16 });
  addContentTitle(s, "CHANGING INHERITED CODE · REFACTOR OR REWRITE", "Six decision factors, never 'how old or ugly'");
  addNotes(s,
    "None of these six factors is how old the code is or how ugly it is, two questions that feel " +
    "diagnostic and mostly aren't. Legacy code that's old and ugly but well covered by tests and fully " +
    "understood is a refactoring candidate in good standing; code written last quarter that nobody can " +
    "explain and has no tests at all is a rewrite candidate despite being brand new. The asymmetry in the " +
    "cost of being wrong is itself a reason most components should default toward refactoring unless a " +
    "specific force argues otherwise: a wrong refactor costs wasted time, a wrong rewrite costs a " +
    "half-built replacement plus a legacy system nobody can touch safely either.");
}

{
  const s = S();
  addContentTitle(s, "CHANGING INHERITED CODE · REFACTOR OR REWRITE", "The strangler fig: three pieces that each earn their keep");
  addBullets(s, [
    "A stable interface: every caller depends on quote(), never on a concrete legacy or modern engine",
    "A routing seam with three modes: legacy-only, compare (logs disagreement, never serves the new answer), and routed (shifts named traffic)",
    "A shared characterization suite: the new engine earns traffic by passing the old engine's own pinned cases, not a fresh set written to match it",
    "Traffic shifts customer by customer, not all at once, so each increment stays observable and reversible",
  ], { fontSize: 17 });
  addNotes(s,
    "Passing a different, newly written test suite proves the new code does what its author intended; " +
    "passing the old engine's own suite proves it does what the business needs, a different and much " +
    "stronger claim. A legacy pricing engine's undocumented customer overrides, a flat-dollar discount for " +
    "one customer and a percentage discount for another, and an unexplained quarter-dollar rounding rule " +
    "inherited from a decommissioned mainframe, are exactly the kind of knowledge a characterization suite " +
    "pulled from the running system captures and a clean rewrite would otherwise silently drop.");
}

{
  const s = S();
  addCodeSlide(s, "CHANGING INHERITED CODE · REFACTOR OR REWRITE", "Compare mode: both engines run, only one answers", "C++ · router",
    [
      "case Mode::Compare: {",
      "    const Quote legacy_quote = legacy_->quote(request);",
      "    const Quote modern_quote = modern_->quote(request);",
      "    if (legacy_quote.total_cents != modern_quote.total_cents) {",
      "        disagreements_.push_back(",
      "            {request, legacy_quote, modern_quote});",
      "    }",
      "    return legacy_quote;  // customer always gets the proven answer",
      "}",
    ],
    "Zero disagreements across the characterization table is the signal that modern is safe to start taking traffic.");
  addNotes(s,
    "Compare mode runs both engines on every request and records any disagreement, while still returning " +
    "the legacy engine's answer, which is the entire reason to run a comparison mode at all rather than " +
    "cutting straight over: a defect introduced during the rewrite cannot reach a customer during the " +
    "verification window. Once the real modern engine runs through compare mode with zero disagreements " +
    "across the full characterization table, a cutover policy starts moving named customers over one at a " +
    "time in routed mode. Every quote printed across the whole migration, from a cold legacy-only start to " +
    "a modern-only finish, stays identical, which is the strangler fig's actual promise.");
}

// =============================================================================
// SECTION 3 - Working with AI and LLM tools
// =============================================================================
divider("03", "Working with AI and LLM tools", "A pattern-literate collaborator, its failure modes, and the gate that catches them",
  "An LLM pairing partner can draft a Strategy, a Factory Method, or a dependency-breaking seam in " +
  "seconds, and it can do it in a shipping system's actual vocabulary if you give it that vocabulary to " +
  "work with. That capability changes how fast a design decision turns into running code. It does not " +
  "change who answers for the code once it ships. This section covers how to prompt with the forces " +
  "vocabulary this deck has used throughout, the three failure modes that show up reliably in generated " +
  "C++, and the two-gate verification discipline, a behavior suite and an idiomatic review, that decides " +
  "whether a draft merges.");

{
  const s = S();
  addContentTitle(s, "WORKING WITH AI & LLM TOOLS", "A collaborator who has read everything and judges nothing");
  addBullets(s, [
    "An LLM has read a vast amount of C++ and applied none of the judgment that decides when a pattern belongs",
    "Naming the force and the candidate pattern narrows the space of plausible answers; a vague request gets a vague, usually over-engineered one back",
    "The same vocabulary works in reverse: a generated factory returning three hardcoded types on an enum hasn't built a Factory Method, it's a switch wearing a factory's name",
    "AI changes who writes the code, not who answers for whether it's correct and idiomatic",
  ], { fontSize: 17 });
  addNotes(s,
    "Phrasing a request as 'the fuel surcharge calculation needs to vary by carrier contract, and the " +
    "rest of checkout shouldn't need to know which rule is active, write this as a Strategy with the " +
    "pattern's usual shape' gives the model the force, the pattern name, and an implicit scope boundary " +
    "all at once, each one narrowing the space of plausible answers. Compare that to 'write a function " +
    "that calculates shipping cost differently per carrier,' which tells the model nothing about whether " +
    "the carrier set is closed or open, so it fills every gap with whatever shape is statistically common " +
    "in a large C++ corpus, which is rarely the minimal one.");
}

{
  const s = S();
  addTwoColBullets(s, [
    "Over-patterning: an interface, a factory, and a singleton for reading one boolean out of one file",
    "Hallucinated APIs: a confident call into an overload that doesn't exist, or belongs to a different library version",
  ], [
    "Non-idiomatic or outdated C++: new/delete where RAII belongs, a typedef instead of using, an output parameter instead of std::optional",
    "None of these fail to compile; idiom and correctness at a boundary are not things a compiler checks",
  ], { fontSize: 16 });
  addContentTitle(s, "WORKING WITH AI & LLM TOOLS", "Three failure modes of an unchecked collaborator");
  addNotes(s,
    "All three share a root cause: a model optimizing for a plausible-looking answer to the prompt rather " +
    "than the smallest correct answer to the actual problem. Over-patterning is this book's own " +
    "anti-pattern material applied to a model trained on a corpus containing plenty of over-engineered " +
    "example code; the fix is the same forces test applied everywhere else, ask what force justifies each " +
    "type the model introduced and delete the ones with no answer. Hallucinated APIs are a predictable " +
    "consequence of how these models generate text, the token sequence that looks most like correct code " +
    "for the surrounding context, with no compiler in the loop until a human or a build catches it.");
}

{
  const s = S();
  addDiagramSlide(s, "WORKING WITH AI & LLM TOOLS", "The human-AI loop with a verification gate", "32-ai-and-llm-tools",
    "The engineer names the forces; AI drafts; nothing ships without the test gate.");
  addNotes(s,
    "The engineer names the forces and the candidate pattern, the AI drafts an implementation, and a " +
    "verification gate of behavior tests plus an idiomatic review decides whether the draft is accepted " +
    "or sent back. Responsibility for correctness stays with the engineer at every point in this loop. " +
    "AI tooling is useful on the other side of this gate too: given an existing function and a request to " +
    "generate a test for every branch it currently takes, asserting whatever it currently returns without " +
    "changing anything, a model produces wide characterization coverage fast. What it should never decide " +
    "unsupervised is whether a given seam is safe to cut in a system with side effects it cannot see the " +
    "full extent of.");
}

{
  const s = S();
  addContentTitle(s, "WORKING WITH AI & LLM TOOLS", "The verify-on-tests gate: two required checks, not either-or");
  addBullets(s, [
    "A behavior test suite, written against the specification the code must satisfy, not against whatever the draft happens to do",
    "A test suite the same model wrote for its own draft inherits its blind spots; a human has to write or review the table",
    "An idiomatic-review pass checks the draft against the project's real conventions: RAII, std::expected over output parameters, concepts over unconstrained templates",
    "Both gates are required: one proves correctness against a spec, the other proves the code fits the codebase it's joining",
  ], { fontSize: 17 });
  addNotes(s,
    "Nothing in the three failure modes is detectable by reading the code and nodding. An over-patterned " +
    "config reader compiles. A hallucinated API sometimes compiles against the wrong overload and runs " +
    "without crashing on the one input a casual smoke test happens to exercise. Non-idiomatic C++ compiles " +
    "by definition, since idiom isn't something the compiler checks. A pattern that passes its behavior " +
    "tests but reads like it was written against an older standard than the rest of the file has still " +
    "failed half of what this discipline checks for, which is exactly why both gates are required and " +
    "neither one is optional.");
}

{
  const s = S();
  addCodeSlide(s, "WORKING WITH AI & LLM TOOLS", "One operator, one boundary, a $101 overcharge", "C++ · Strategy",
    [
      "// Correct: lower bounds inclusive",
      "UsageTier Classify(long monthly_requests) const {",
      "    if (monthly_requests < 10'000)  return UsageTier::Starter;",
      "    if (monthly_requests < 100'000) return UsageTier::Growth;",
      "    return UsageTier::Scale;",
      "}",
      "",
      "// AI-plausible: one operator wrong on the first boundary",
      "UsageTier Classify(long monthly_requests) const {",
      "    if (monthly_requests <= 10'000) return UsageTier::Starter; // bug",
      "    if (monthly_requests < 100'000) return UsageTier::Growth;",
      "    return UsageTier::Scale;",
      "}",
    ],
    "At exactly 10,000 requests the bug bills $200 instead of $99: a $101 overcharge from one character.");
  addNotes(s,
    "Both implementations have identical structure, an identical number of branches, and the same second " +
    "comparison; only the operator on the first branch differs by one character, and <= reads as an " +
    "entirely reasonable choice for an inclusive-sounding tier name if you're not holding the exact spec " +
    "wording in your head while reviewing it. A boundary table, every tier's low end, middle, and high " +
    "end, catches the single disagreeing case at exactly 10,000 requests; every other case still matches, " +
    "which is exactly what makes a one-operator bug this easy to miss on a casual read and this reliable " +
    "to catch with a table that tests every boundary rather than one value comfortably inside each tier.");
}

// =============================================================================
// SECTION 4 - Putting it together
// =============================================================================
divider("04", "Putting it together", "The final project: five patterns composed into one shipment dispatch system",
  "Every pattern up to this point has been taught the same way: one force, one worked example, one " +
  "focused test suite. That's the right way to learn what a pattern buys you in isolation. It's not how " +
  "these patterns show up in a real system. This section builds a shipment dispatch console that a " +
  "warehouse operator could plausibly run, composing Strategy for rate selection, Factory for carrier " +
  "creation, State for the shipment lifecycle, Command for reversible operator actions, and Observer for " +
  "lifecycle notifications. The focus is the four seams where these patterns touch, not re-teaching any " +
  "one of them from scratch.");

{
  const s = S();
  addContentTitle(s, "PUTTING IT TOGETHER", "One dispatcher, five patterns, four seams");
  addBullets(s, [
    "ShipmentDispatcher is the one type that knows about all five composed patterns; every other type sees only the interface directly beside it",
    "Factory hands back a Strategy: CarrierFactory resolves a carrier code to a profile carrying the right RateStrategy, and the caller never names a concrete strategy",
    "Command drives State: AdvanceLifecycleCommand wraps one call to Shipment::advance(); undo restores a snapshot rather than computing a reverse event",
    "State fires Observer: a legal transition calls notify(), and every subscriber reacts without knowing a Command was ever involved",
  ], { fontSize: 17 });
  addNotes(s,
    "A real shipment dispatch console doesn't have a Strategy module and a separate State module that " +
    "never talk to each other; it has one Shipment whose lifecycle fires events that something else " +
    "reacts to, one rate calculation something else invokes on demand, and one carrier-selection step " +
    "that hands the right pieces to everything downstream of it. It's tempting to assume Command's undo " +
    "for a State transition should walk the machine backward through a reverse event; real carriers don't " +
    "support that, so undo captures a status snapshot before the change and restores it directly, the " +
    "same technique this book applies to any action with no clean arithmetic inverse.");
}

{
  const s = S();
  addDiagramSlide(s, "PUTTING IT TOGETHER", "Five patterns, composed", "31-final-project",
    "Five patterns composed into one dispatcher, with the seams where they meet.");
  addNotes(s,
    "CarrierFactory::create() returns a CarrierProfile already carrying a std::unique_ptr<RateStrategy> " +
    "pointed at the correct concrete implementation; nothing calling the factory ever names a concrete " +
    "strategy type directly. A new carrier with a third pricing rule needs one new RateStrategy subclass " +
    "and one new branch inside the factory; ShipmentDispatcher itself doesn't change at all, because it " +
    "was never written against a concrete strategy in the first place. That's the Factory-to-Strategy " +
    "seam doing its job: the factory's whole purpose is choosing which strategy a caller gets, and the " +
    "caller never has to know the choice was made.");
}

{
  const s = S();
  addTwoColBullets(s, [
    "Factory -> Strategy: a new carrier means one new RateStrategy subclass and one new branch; the dispatcher never changes",
    "Command -> State: every lifecycle change becomes undoable by construction, not just for the actions someone remembered to wrap",
  ], [
    "Command -> Strategy: a weight-correction command asks the same RateStrategy instance that priced the opening quote; they can't disagree",
    "State -> Observer: a dashboard, a billing ledger, and a notification service all react to the same event with no reference to each other",
  ], { fontSize: 16 });
  addContentTitle(s, "PUTTING IT TOGETHER", "The four seams, named");
  addNotes(s,
    "An operator console only reaches shipment.advance() through a command the dispatcher builds, and the " +
    "dispatcher's command history is what runs it, which is what makes every lifecycle change undoable by " +
    "construction rather than only for the operations someone remembered to wrap. AdjustWeightCommand's " +
    "execute() asks the exact same RateStrategy object the factory built for this shipment's carrier for a " +
    "fresh quote at the corrected weight, which is the whole point of composing Factory and Strategy " +
    "before composing Command on top of them: a weight correction and a shipment's opening quote are " +
    "always priced by the identical rule because they are, literally, the same object.");
}

{
  const s = S();
  addDiagramSlide(s, "PUTTING IT TOGETHER", "One shipment's run, in order", "31-final-project-sequence",
    "One shipment's run: the same RateStrategy prices the open quote and the correction.");
  addNotes(s,
    "The demo opens a shipment with one carrier, prices the opening quote through the factory-built " +
    "strategy, advances the lifecycle through two legal transitions while two observer notifications " +
    "fire, corrects the weight through a command that recomputes against the same strategy instance, " +
    "undoes that correction and watches both the weight and the quote restore exactly, delivers the " +
    "shipment, and finally rejects an operator's mis-scanned repeat pickup because the state machine's " +
    "own transition table refuses an event that no longer applies. Every number traces back to one of " +
    "the four seams on the previous slide.");
}

{
  const s = S();
  addCodeSlide(s, "PUTTING IT TOGETHER", "The composition root: Factory, Strategy, and Command meeting", "C++ · dispatcher",
    [
      "CarrierProfile profile = factory_.create(request);",
      "shipment_.emplace(shipment_id, profile.carrier_name,",
      "                   request.destination_zone, weight_kg, Cents{0});",
      "shipment_->set_quoted_rate_cents(",
      "    profile.rate_strategy->quote(*shipment_));",
      "",
      "std::unique_ptr<ShipmentCommand>",
      "make_adjust_weight_command(double new_weight_kg) {",
      "    return std::make_unique<AdjustWeightCommand>(",
      "        shipment(), *carrier_profile().rate_strategy, new_weight_kg);",
      "}",
    ],
    "A weight correction and the opening quote always price through the identical RateStrategy instance.");
  addNotes(s,
    "ShipmentDispatcher is the one type in this whole system that knows about all five patterns at once; " +
    "CarrierFactory doesn't know about Shipment, AdjustWeightCommand doesn't know about CarrierFactory, " +
    "and no LifecycleObserver implementation knows ShipmentCommand exists. make_adjust_weight_command() " +
    "closes over the exact same strategy object the factory built for this shipment's carrier, which " +
    "means a weight correction and the shipment's opening quote can never drift apart, by construction, " +
    "not by convention. That single line is the payoff of composing Factory and Strategy before " +
    "composing Command on top of both.");
}

// =============================================================================
// Close
// =============================================================================
{
  const s = S();
  addContentTitle(s, "CLOSING", "Test it, change it safely, let AI accelerate");
  addBullets(s, [
    "Test it: GoogleTest and gMock give every pattern a proof, not just a compile",
    "Change it safely: characterize, open a seam, then refactor, with the suite green at every step",
    "Let AI accelerate: name the forces and the pattern, then gate every draft on a behavior suite and an idiomatic review",
    "The judgment that stays with the engineer: whether a problem needed a pattern at all, and who answers for the code once it ships",
  ], { fontSize: 17 });
  addNotes(s,
    "A pattern catalogue by itself only ever answers 'what shape should this code take.' This deck covers " +
    "the three questions a catalogue alone never answers: does it actually do what it claims, is it safe " +
    "to change once it's inherited, and how do you get useful work out of an AI pairing partner without " +
    "handing it the one decision it has no stake in. An LLM can draft any pattern in this book's " +
    "catalogue in seconds and can draft the test suite that checks its own draft just as fast. It can't " +
    "tell you whether the problem in front of you needed a pattern at all, and that judgment call is the " +
    "one thing this deck cannot delegate to tooling, now or later.");
}

// =============================================================================
// Appendix
// =============================================================================
divider("05", "Appendix", "Reference tables: the test-double taxonomy and the dependency-breaking catalogue",
  "Two lookup tables for after the talk. The first maps Meszaros's five-kind test-double taxonomy, " +
  "dummy, stub, spy, mock, fake, onto the exact gMock and hand-written-fake vocabulary this deck's " +
  "examples use. The second is the full 24-entry dependency-breaking catalogue from the changing-" +
  "inherited-code section, each technique paired with the modern C++20 or C++23 tool that expresses it " +
  "today. Neither table introduces new material; both exist purely as a reference to return to once a " +
  "real piece of inherited code is in front of you and you need the exact name for the seam it calls for.");

{
  const s = S();
  addStatusTable(s, [
    { code: "Dummy", name: "Satisfies a signature, never exercised", purpose: "A QuoteLedger& a test never inspects because that code path never calls it" },
    { code: "Stub", name: "Returns canned values, no call verification", purpose: "ON_CALL(clock, now()).WillByDefault(Return(fixed_instant))" },
    { code: "Spy", name: "Records calls, inspected after the fact", purpose: "A hand-written SpyCarrierGateway appending every request to a vector" },
    { code: "Mock", name: "Verifies an expected interaction, fails if unmet", purpose: "gMock's EXPECT_CALL on MockQuoteLedger::record()" },
    { code: "Fake", name: "A working, lightweight implementation", purpose: "An InMemoryTrackingStore backed by a hash map, not a real connection" },
  ], { colW: [1.70, 4.20, 6.19], rowH: 0.57 });
  addContentTitle(s, "APPENDIX", "The test-double taxonomy, mapped to this stack");
  addNotes(s,
    "The dividing line between a mock and a fake is usually this: if the next sentence after 'this test " +
    "checks that' names a method and an argument, reach for a mock; if it names a value the collaborator " +
    "should be holding afterward, reach for a fake. A dummy and a stub are easy to confuse too: a dummy " +
    "is never exercised at all, while a stub is called and answers with a canned value the test never " +
    "verifies the call count or arguments of. A spy sits between a stub and a mock, recording what " +
    "happened for the test to inspect after exercise, with no EXPECT_CALL and no expectation declared up " +
    "front.");
}

{
  const s = S();
  addDenseRefTable(s, [
    { code: "Adapt Parameter", name: "Extract a minimal interface; wrap the type", purpose: "Heavyweight parameter, only a few fields read" },
    { code: "Break Out Method Object", name: "A small class/local struct with operator()", purpose: "A long method's locals resist isolation" },
    { code: "Definition Completion", name: "A test-only TU supplies the definition", purpose: "A declared symbol has no linkable definition" },
    { code: "Encapsulate Global References", name: "Wrap the global behind an interface", purpose: "Inline global reads/writes, scattered everywhere" },
    { code: "Expose Static Method", name: "Move logic to a static or free function", purpose: "Instance-free logic trapped in an instance method" },
    { code: "Extract and Override Call", name: "Virtual override (C++20) or deducing-this (C++23)", purpose: "One troublesome call sits inline in a method" },
    { code: "Extract and Override Factory Method", name: "A virtual/deducing-this factory method", purpose: "An embedded new hard-wires the concrete type" },
    { code: "Extract and Override Getter", name: "A virtual/deducing-this getter", purpose: "A troublesome expression inlined at every use" },
    { code: "Extract Implementer", name: "Split into interface + implementer", purpose: "A contract mixed with a varying implementation" },
    { code: "Extract Interface", name: "Pull the minimal contract into an abstract base", purpose: "Only one concrete collaborator type is buildable" },
    { code: "Introduce Instance Delegator", name: "A thin instance class forwards to the static API", purpose: "A static-only API called from many places" },
    { code: "Introduce Static Setter", name: "A static setter with an RAII reset guard", purpose: "A global needs substituting with no write path" },
  ]);
  addContentTitle(s, "APPENDIX", "The dependency-breaking catalogue, 1 of 2");
  addNotes(s,
    "This is the first half of Feathers' full 24-technique catalogue, each one modernized to the " +
    "C++20/23 tool that expresses it today. Several deserve a note beyond the table: Extract and Override " +
    "Call is the one entry with a genuine standard-to-standard difference, a protected virtual method a " +
    "test subclass overrides at runtime under C++20, or a deducing-this hook resolved at compile time " +
    "under C++23 with no vtable at all. Reach for Parameterize Constructor or Parameterize Method, on the " +
    "next slide, before any of these whenever a dependency already enters through a parameter list a " +
    "caller controls; nothing beats a plain argument for simplicity.");
}

{
  const s = S();
  addDenseRefTable(s, [
    { code: "Link Substitution", name: "A test-only TU provides the same symbols", purpose: "Replace a dependency at the object-file level" },
    { code: "Parameterize Constructor", name: "A defaulted constructor parameter", purpose: "A constructor builds its own collaborator internally" },
    { code: "Parameterize Method", name: "Pass the collaborator as a method argument", purpose: "One method reaches for a hard-wired collaborator" },
    { code: "Primitivize Parameter", name: "Accept the primitive values directly", purpose: "A few primitives hide inside a heavyweight parameter" },
    { code: "Pull Up Feature", name: "Move the feature to a shared base class", purpose: "A needed method lives on a dependency-heavy subclass" },
    { code: "Push Down Dependency", name: "Move the dependency into the using subclasses", purpose: "A base class carries a dependency only some subclasses need" },
    { code: "Replace Function with Function Pointer", name: "A std::function/move_only_function member", purpose: "A hard C-style call no OO seam can intercept" },
    { code: "Replace Global Reference with Getter", name: "A virtual or injected getter", purpose: "A direct global read sits inline in a method body" },
    { code: "Subclass and Override Method", name: "A test-only subclass overrides just that method", purpose: "Any method whose implementation a test must suppress" },
    { code: "Supersede Instance Variable", name: "A setter replaces the collaborator in place", purpose: "No constructor seam exists yet" },
    { code: "Template Redefinition", name: "Parameterize the class on the collaborator type", purpose: "A compile-time-only seam for the collaborator type" },
    { code: "Text Redefinition", name: "A preprocessor seam, a last resort", purpose: "No structural technique above it is available" },
  ]);
  addContentTitle(s, "APPENDIX", "The dependency-breaking catalogue, 2 of 2");
  addNotes(s,
    "Template Redefinition is the technique C++ is unusually well suited for among the languages Feathers " +
    "originally wrote against: a class template parameterized on its collaborator type gets a compile-" +
    "time seam with no virtual call and no interface to maintain, at the cost of every client needing to " +
    "know the template parameter. Text Redefinition sits at the opposite end: a last resort even in " +
    "languages where it's easy, and a #define-based seam in C++ carries real risk of violating the one-" +
    "definition rule across translation units the moment two translation units see different values for " +
    "the same symbol. Reach for it only once every technique above it on both tables has been ruled out.");
}

// =============================================================================
pres.writeFile({ fileName: OUT })
  .then(p => console.log("WROTE", p))
  .catch(e => { console.error(e); process.exit(1); });
