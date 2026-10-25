// deck-201.js -- "Design Patterns with C++" / 201: Idiomatic C++ Implementations
// Red Hat-branded 16:9 companion deck. Build: node deck-201.js (from this dir).

"use strict";

const H = require("./deck-helpers.js");
const {
  COLOR, FONT, W, ASSETS,
  newDeck, addFooter, addContentTitle, addBullets, addTwoColBullets,
  addStatusTable, addCaption, addCodeSlide, addDiagramSlide, addSectionDivider, addNotes,
} = H;

const OUT = "./cpp-design-patterns-201-idiomatic-cpp.pptx";
const REV = "r01.0";

const pres = newDeck();
pres.title = "Design Patterns with C++ -- 201: Idiomatic C++ Implementations";
let pageNum = 0;

function S() {
  const s = pres.addSlide();
  pageNum += 1;
  addFooter(s, pageNum);
  return s;
}
function divider(code, title, subtitle, notes) {
  const s = pres.addSlide();
  pageNum += 1;
  addSectionDivider(s, code, title, subtitle);
  addNotes(s, notes);
}

// ============================================================================
// COVER
// ============================================================================
{
  const s = pres.addSlide();
  pageNum += 1;
  s.background = { color: COLOR.white };
  try { s.addImage({ path: `${ASSETS}/cover-panel.png`, x: 0, y: 0, w: W, h: 7.5 }); } catch (e) {}
  s.addText("C++20 / C++23 COMPANION DECK", {
    x: 6.00, y: 1.98, w: 6.90, h: 0.34,
    fontFace: FONT.title, fontSize: 14, bold: true, color: COLOR.red, charSpacing: 6,
    align: "left", valign: "middle",
  });
  s.addText([
    { text: "Design Patterns with C++", options: { breakLine: true } },
    { text: "201: Idiomatic C++ Implementations" },
  ], {
    x: 5.95, y: 2.20, w: 6.95, h: 2.65, fontFace: FONT.title, fontSize: 36, bold: true,
    color: COLOR.ink, align: "left", valign: "top", lineSpacingMultiple: 1.08,
  });
  s.addText("How the patterns you already know get expressed in modern C++20 and C++23.", {
    x: 6.00, y: 5.05, w: 6.70, h: 0.75,
    fontFace: FONT.body, fontSize: 18, italic: true, color: COLOR.caption, align: "left", valign: "top",
  });
  s.addText(REV, { x: 11.85, y: 5.85, w: 0.95, h: 0.30, fontFace: FONT.mono, fontSize: 11, color: COLOR.caption, align: "right", valign: "middle" });
  try { s.addImage({ path: `${ASSETS}/logo-candidate-2.png`, x: 11.10, y: 6.80, w: 1.55, h: 0.37 }); } catch (e) {}
  addNotes(s,
    "This deck is for engineers who already know Strategy, State, Command, Factory, and Builder " +
    "conceptually and want to see how modern C++ actually expresses them: RAII and ownership as the " +
    "substrate every idiom sits on, CRTP and policy-based design as compile-time alternatives to a " +
    "vtable, type erasure and PImpl as the two answers to hiding a concrete type, and the C++20-to-" +
    "C++23 upgrades that remove a workaround or add a typed failure path without changing the shape " +
    "of the pattern underneath. Every code slide is a trimmed, real snippet from a shipping and " +
    "logistics domain: dock bays, carrier contracts, freight manifests, tracking ledgers. Nothing here " +
    "is Foo or Bar.");
}

// ============================================================================
// SECTION 00: THE C++ SUBSTRATE
// ============================================================================
divider("00", "The C++ substrate",
  "RAII, ownership, and the test discipline every idiom in this deck builds on.",
  "This section is the foundation, not the destination: RAII ties a resource's lifetime to an " +
  "object's lifetime so cleanup cannot be forgotten on any exit path, value semantics and smart " +
  "pointers make an object graph's ownership explicit in the type instead of in a comment, and " +
  "GoogleTest is the one testing idiom every pattern in this deck is verified against. Keep this " +
  "section brief in the room: it is review for this audience, but every idiomatic expression that " +
  "follows, CRTP, type erasure, the pattern rewrites, depends on the reader already trusting these " +
  "three mechanisms.");

{
  const s = S();
  addContentTitle(s, "SUBSTRATE · RAII", "A resource's lifetime is an object's lifetime");
  addBullets(s, [
    "A dock bay lease acquires in its constructor and releases, unconditionally, in its destructor. No public release() method exists to forget.",
    "A thrown exception does not skip destructors. Stack unwinding runs every fully constructed local object's destructor on the way out, the lease's included.",
    "Copying a resource guard is deleted outright; moving it transfers ownership and leaves the source with nothing to release, so a double-release is not just unlikely, it is unrepresentable.",
    "A general-purpose ScopeGuard generalizes the same shape to any callable, with a dismiss() that cancels the cleanup once an operation has provably succeeded.",
  ], { fontSize: 17 });
  addNotes(s,
    "Lead with the dock-bay example: a naive bool flag needs a matching reset on every exit path, " +
    "and a reviewer has to trace every branch by hand to confirm none of them forgot it. RAII replaces " +
    "that discipline with a language guarantee: the destructor runs when the scope ends, whether that " +
    "end is a normal return, an early return, or a thrown exception. Emphasize the move-only design: a " +
    "copied guard would let two objects believe they both own the same resource, and the second " +
    "destructor would release something already released. ScopeGuard is the generalization worth " +
    "remembering, arm the undo first, do the risky work, cancel the undo only once the work succeeded.");
}
{
  const s = S();
  addDiagramSlide(s, "SUBSTRATE · RAII", "Acquire in the constructor, release in the destructor",
    "05-raii-lifecycle",
    "A thrown exception still funnels into the same release point as a clean exit.");
  addNotes(s,
    "Walk the diagram left to right: a lease acquires a bay in its constructor, the happy path returns " +
    "normally and the destructor releases, and the exception path still ends at the same destructor " +
    "during stack unwinding. The point worth landing is that the destructor call is inserted by the " +
    "compiler at every point control can leave the scope, including the ones a throw creates, so there " +
    "is no second code path to write or forget. This is the exact guarantee a finally block in another " +
    "language reaches for at the call site; RAII builds it into the type itself, so every caller gets " +
    "it automatically.");
}
{
  const s = S();
  addDiagramSlide(s, "SUBSTRATE · RAII", "A move transfers ownership, not a copy",
    "05-move-ownership-transfer",
    "The source's owns_bay flag flips to false; the registry's release count still advances by exactly one.");
  addNotes(s,
    "A move constructor copies the source's state into the new object and then flips the source's " +
    "owns_bay flag to false. Both objects still destruct, but only one of them finds anything to " +
    "release; the moved-from object's destructor short-circuits into a no-op. The quantitative proof " +
    "worth mentioning: a test moves a lease and lets both the moved and moved-from objects destruct, " +
    "then asserts the registry's release count is exactly one, not zero and not two. That is the same " +
    "guarantee std::unique_ptr gives a raw pointer, a move transfers responsibility for releasing " +
    "exactly once, no matter how many objects the ownership passed through.");
}
{
  const s = S();
  addCodeSlide(s, "SUBSTRATE · RAII", "Reporting a busy resource: throw, or a typed value", "C++20 · C++23",
    [
      "// C++20: acquiring a dock bay can only report a conflict by",
      "// throwing. Every caller who wants to avoid a try block has no",
      "// alternative entry point to reach for.",
      "DockBayLease lease(registry, bay_id);  // throws BayUnavailableError",
      "",
      "// C++23: std::expected reports the same conflict as a typed",
      "// value, checkable with an if and no exception machinery.",
      "std::expected<DockBayLease, std::string> result =",
      "    DockBayLease::try_acquire(registry, bay_id);",
      "if (!result.has_value()) {",
      "    std::cout << \"Rejected: \" << result.error() << '\\n';",
      "}",
    ],
    "Both paths protect the same invariant; only how a caller learns about a conflict changes.",
    { fontSize: 11 });
  addNotes(s,
    "Both columns release through the identical destructor once a DockBayLease exists; RAII's core " +
    "guarantee does not move between standards. What changes is the failure contract: C++20 forces " +
    "every caller to decide between wrapping the call in a try block or accepting an unhandled " +
    "exception on a request path. C++23's std::expected gives a second entry point that reports the " +
    "identical rejection as a checkable value with zero exception machinery. Frame this as additive, " +
    "not a replacement: the throwing constructor stays the default; try_acquire is the C++23 option " +
    "for a caller who specifically wants to avoid a try block.");
}
{
  const s = S();
  addContentTitle(s, "SUBSTRATE · OWNERSHIP", "Four ownership shapes, one object graph");
  addBullets(s, [
    "A Parcel is a plain value type with the Rule of Zero: every member already copies and moves correctly, so Parcel writes no special member of its own.",
    "A dock slot holds a parcel through unique_ptr: sole ownership, transferred by move, never shared.",
    "A carrier contract is shared through shared_ptr: more than one route quotes against the same negotiated rate table, and renegotiating it through one route is visible to every route.",
    "A manifest tree's child-to-parent edge is weak_ptr, not shared_ptr, specifically to avoid a reference cycle that would keep both objects alive forever.",
  ], { fontSize: 17 });
  addNotes(s,
    "The ownership question is separate from the lifetime question RAII already answered: RAII says " +
    "when something releases, this says who is allowed to hold it and how many owners there are at " +
    "once. Four types, four different answers, driven by the actual shape of the object graph rather " +
    "than by style. Call out the reflex to reach for shared_ptr by default: it compiles for a sole-" +
    "owner case too, but it silently permits a second owner the design never intended, where " +
    "unique_ptr's deleted copy constructor would have refused to compile the moment that happened.");
}
{
  const s = S();
  addDiagramSlide(s, "SUBSTRATE · OWNERSHIP", "Sole, shared, and observing ownership in one graph",
    "06-ownership-model",
    "unique_ptr as a sole owner, shared_ptr as a ref-counted shared owner, weak_ptr observing without owning.");
  addNotes(s,
    "Three smart pointers, three distinct ownership topologies, in the same warehouse dispatch domain. " +
    "unique_ptr models a dock slot holding exactly one parcel at a time. shared_ptr models a carrier " +
    "contract more than one route depends on, with a reference count that only frees the contract when " +
    "the last route stops quoting against it. weak_ptr models a manifest node's reverse edge back to " +
    "its parent, observing without contributing to the reference count, which is exactly what breaks " +
    "the cycle a second shared_ptr in that direction would create.");
}
{
  const s = S();
  addDiagramSlide(s, "SUBSTRATE · OWNERSHIP", "Value semantics versus reference semantics",
    "06-value-vs-reference",
    "A copied parcel stays independent; two routes sharing one carrier contract both see a renegotiated rate.");
  addNotes(s,
    "This is the contrast worth drilling: copying a Parcel produces two independent objects, mutate " +
    "the copy's weight and the original is untouched, because every member it holds deep-copies on its " +
    "own. Two ShippingRoute objects built from the same shared_ptr<CarrierContract> are the opposite: " +
    "renegotiate a rate through one route's contract and the other route's very next quote reflects the " +
    "new rate immediately, because there is exactly one CarrierContract in memory and both routes point " +
    "at it. A type's ownership model decides which behavior a caller gets, not how the caller happens " +
    "to write the assignment.");
}
{
  const s = S();
  addCodeSlide(s, "SUBSTRATE · OWNERSHIP", "Adopting a raw pointer from a legacy C factory", "C++20 · C++23",
    [
      "// C++20: a named raw pointer sits unmanaged between the call and",
      "// the handle's construction. Nothing stops a line inserted later",
      "// in that gap from leaking it.",
      "CRateTable* raw_table = nullptr;",
      "legacy_open_rate_table(carrier_code.c_str(), &raw_table);",
      "return RateTableHandle{raw_table};",
      "",
      "// C++23: std::out_ptr writes straight into the handle's own",
      "// storage. No bare pointer ever exists outside a unique_ptr's",
      "// control.",
      "RateTableHandle handle;",
      "legacy_open_rate_table(carrier_code.c_str(), std::out_ptr(handle));",
      "return handle;",
    ],
    "std::inout_ptr is the companion for a C API that also reads the handle's current pointer first.",
    { fontSize: 11 });
  addNotes(s,
    "Both versions produce the identical RateTableHandle, a unique_ptr with a deleter that calls the " +
    "legacy close function. The difference is how long a bare, unmanaged CRateTable pointer exists in " +
    "the function body. The C++20 version has a real local variable with real scope; anything inserted " +
    "into that gap later, another call, an early return, a thrown exception, can leak the table before " +
    "the handle is ever constructed. std::out_ptr produces a temporary that writes directly into the " +
    "handle when the call returns, so that window never opens. This is a narrow, single-call-site " +
    "refinement; it does not change the ownership model, only how briefly a raw pointer is exposed.");
}
{
  const s = S();
  addDiagramSlide(s, "SUBSTRATE · TESTING", "Five GoogleTest styles, one question each",
    "07-test-styles",
    "Fixture, parameterized, typed, mock-interaction, and death tests, all funneling into the same CTest run.");
  addNotes(s,
    "Keep this brief; it is the throughline, not a destination. Every pattern rewrite later in this " +
    "deck is verified by one of these five shapes: a TEST_F fixture for shared mutable state, TEST_P " +
    "for a table of input-to-outcome cases, a TYPED_TEST_SUITE for a contract more than one concrete " +
    "type must honor, a GoogleMock EXPECT_CALL for an interaction a return-value check cannot see, and " +
    "EXPECT_DEATH for a precondition that must abort rather than degrade gracefully. The habit worth " +
    "naming: ASSERT_* stops the test on the spot because continuing would be undefined behavior; " +
    "EXPECT_* lets it continue because two independent facts about the same result both deserve to run.");
}

// ============================================================================
// SECTION 01: IDIOMATIC C++ EXPRESSIONS
// ============================================================================
divider("01", "Idiomatic C++ expressions",
  "The compile-time and value-semantic machinery that makes a C++ design distinct from any other language's.",
  "This section covers three idioms that have no direct equivalent in a language without templates: " +
  "CRTP and policy-based design resolve polymorphism entirely at compile time, with no vtable " +
  "anywhere. Type erasure and PImpl are two different answers to hiding a concrete type, one behind a " +
  "value, one behind a pointer to an incomplete type. Expression templates defer arithmetic into a " +
  "tree that evaluates once, and a self-registering registry removes the one central file every new " +
  "concrete type would otherwise have to touch. None of these replace the patterns the audience " +
  "already knows; they are the vocabulary modern C++ adds underneath them.");

{
  const s = S();
  addContentTitle(s, "IDIOMS · STATIC POLYMORPHISM", "A vtable is a choice, not a requirement");
  addBullets(s, [
    "CRTP: a base class template that resolves to a derived type's method through static_cast, decided once, at compile time, with no virtual function anywhere.",
    "The same self-type trick works as a reusable mixin with no connection to any one class hierarchy; std::enable_shared_from_this is the standard library's own example of it.",
    "Policy-based design composes a host class from more than one independent, concept-constrained template parameter, each swappable without touching the others.",
    "The trade: zero runtime dispatch cost, paid for with a choice fixed before the program starts and one compiled instantiation per combination a build actually uses.",
  ], { fontSize: 17 });
  addNotes(s,
    "Set up the scenario this pays off in: a pricing library built once per deployment region " +
    "with that region's tariff policy fixed at compile time, where every call-site indirection on a " +
    "latency-sensitive path is a cost worth avoiding. CRTP and policy-based design both answer that " +
    "with zero runtime cost, no vtable lookup, no type-erased callable's heap allocation. The forces " +
    "flip back toward virtual dispatch the moment the choice has to be made after the program starts, " +
    "from a configuration file, a plugin, or a user's runtime selection.");
}
{
  const s = S();
  addDiagramSlide(s, "IDIOMS · STATIC POLYMORPHISM", "CRTP and policy composition at compile time",
    "21-crtp-policy",
    "CRTP resolves a derived type without a vtable; a policy host composes from interchangeable compile-time policies.");
  addNotes(s,
    "Point at the static_cast in the CRTP base: at the point quote() is instantiated for a specific " +
    "derived type, the compiler already knows the concrete type, so the cast is a reinterpretation the " +
    "type system guarantees is safe, not a runtime check. On the policy side, two independent template " +
    "parameters, a rounding policy and a surcharge policy, compose by simple instantiation rather than " +
    "by a branch or a flag the host class would otherwise have to grow. Each distinct combination is a " +
    "separate, fully resolved type the compiler can see straight through and inline.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · STATIC POLYMORPHISM", "CRTP versus deducing this", "C++20 · C++23",
    [
      "// C++20: CRTP. The base names Derived as a template argument",
      "// and casts *this down to it explicitly.",
      "template <typename Derived>",
      "class RateModel {",
      "public:",
      "    constexpr Cents quote(const Shipment& s) const {",
      "        return static_cast<const Derived*>(this)->compute(s);",
      "    }",
      "};",
      "class FlatRateModel : public RateModel<FlatRateModel> { /* ... */ };",
      "",
      "// C++23: deducing this. An ordinary, non-template base; the",
      "// compiler deduces the most-derived type at the call site.",
      "class DeducedRateModel {",
      "public:",
      "    template <typename Self>",
      "    constexpr Cents quote(this const Self& self, const Shipment& s) {",
      "        return self.compute(s);",
      "    }",
      "};",
    ],
    "Both compile to the same zero-overhead call; deducing this removes the template argument and the cast.",
    { fontSize: 10 });
  addNotes(s,
    "Both columns resolve entirely at compile time with no vtable in either one; a test cross-checks " +
    "that both produce the identical quote for the identical shipment. What deducing this removes is " +
    "exactly the two constructions a reader has to have explained once before they stop looking " +
    "strange: the self-referential template argument in the inheritance list, and the explicit " +
    "static_cast inside the base. What it does not change is the underlying answer to when static " +
    "polymorphism is the right tool at all: the choice is still fixed at compile time either way, and " +
    "neither version buys back runtime swappability.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · STATIC POLYMORPHISM", "Two independent policy axes, composed", "C++20 concepts",
    [
      "template <typename Policy>",
      "concept RoundsCents = requires(const Policy& p, Cents c) {",
      "    { p.round(c) } -> std::convertible_to<Cents>;",
      "};",
      "",
      "template <RoundsCents Rounding, SurchargesShipment Surcharge>",
      "class ShippingCalculator {",
      "public:",
      "    constexpr Cents quote(const Shipment& shipment) const {",
      "        const Cents raw = base_cents_ + surcharge_policy_.surcharge(shipment);",
      "        return rounding_policy_.round(raw);",
      "    }",
      "private:",
      "    Cents base_cents_;",
      "    Rounding rounding_policy_;",
      "    Surcharge surcharge_policy_;",
      "};",
      "// ShippingCalculator<RoundUpToNickelPolicy, FlatSurchargePolicy> and",
      "// <RoundUpToNickelPolicy, DeclaredValueSurchargePolicy> are two",
      "// fully resolved types; swapping one axis never touches the other.",
    ],
    "A third independent decision would add a third template parameter, not a branch inside quote().",
    { fontSize: 10 });
  addNotes(s,
    "The concepts constraining Rounding and Surcharge are doing real work: without them, a caller who " +
    "passes a type missing round() gets a wall of template instantiation errors from deep inside " +
    "ShippingCalculator; with them, the compiler reports a clear concept-check failure naming exactly " +
    "what was missing. The design point worth stressing is that a second independent axis gets its own " +
    "template parameter rather than a flag or a second constructor bolted onto the first policy. The " +
    "cost side of this trade is real too: every distinct combination of Rounding and Surcharge " +
    "instantiates its own compiled copy, which is a binary-size and build-time cost worth measuring " +
    "once the policy space grows past a handful of axes.");
}
{
  const s = S();
  addContentTitle(s, "IDIOMS · HIDING A CONCRETE TYPE", "Type erasure and the PImpl firewall");
  addBullets(s, [
    "Type erasure holds an unbounded family of unrelated concrete types behind one fixed-size value, with no shared base class anywhere, the same technique std::function and std::any use internally.",
    "A hand-written concept/model pair is the mechanism: a private polymorphic concept, a template model that adapts one concrete type to it, a unique_ptr as the only member a caller ever sees.",
    "PImpl hides implementation detail behind a pointer to an incomplete type, keeping a public class's layout at exactly one pointer no matter how much its implementation grows.",
    "The two solve different problems: type erasure for types you do not control, PImpl for a closed, stable interface over a changeable implementation.",
  ], { fontSize: 17 });
  addNotes(s,
    "Set up the checkout scenario: several unrelated rate quoters arrive in the same request, a flat-" +
    "rate table, a weight-based service, a one-off promotional quoter, none written against a shared " +
    "interface and none needing to be, because checkout code only ever calls one method on any of them. " +
    "Type erasure is the answer when the concrete types are not yours to retrofit inheritance onto. " +
    "PImpl answers a different, unrelated problem for the same domain: a quoting ledger whose storage " +
    "detail should never leak into every caller's recompile, and whose public layout needs to stay " +
    "stable as a library ships across releases.");
}
{
  const s = S();
  addDiagramSlide(s, "IDIOMS · HIDING A CONCRETE TYPE", "No shared base class, no exposed implementation",
    "22-type-erasure-pimpl",
    "A value-semantic wrapper erases unrelated quoters; a PImpl firewall hides a ledger behind one pointer.");
  addNotes(s,
    "On the left, AnyRateQuoter holds whatever concrete quoter a caller hands it, with no template " +
    "parameter of its own, so two AnyRateQuoter values built from different concrete types sit side by " +
    "side in one vector with the identical static type. On the right, RateQuoteLedger exposes exactly " +
    "one pointer regardless of how much bookkeeping its hidden Impl grows to hold, which is what makes " +
    "a library's public ABI stable across point releases. Call out that AnyRateQuoter could never be " +
    "put behind PImpl itself: its constructor is a template, and a template's definition has to stay " +
    "visible everywhere it is instantiated.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · HIDING A CONCRETE TYPE", "AnyRateQuoter: a concept, a model, one pointer", "C++20",
    [
      "template <typename Quoter>",
      "concept Quotable = requires(const Quoter& q, const Shipment& s) {",
      "    { q.quote(s) } -> std::convertible_to<Cents>;",
      "};",
      "",
      "class AnyRateQuoter {",
      "public:",
      "    template <Quotable Quoter>",
      "    AnyRateQuoter(Quoter quoter)",
      "        : model_(std::make_unique<Model<Quoter>>(std::move(quoter))) {}",
      "    Cents quote(const Shipment& s) const { return model_->quote(s); }",
      "private:",
      "    struct Concept {",
      "        virtual Cents quote(const Shipment&) const = 0;",
      "        virtual std::unique_ptr<Concept> clone() const = 0;",
      "    };",
      "    template <Quotable Quoter>",
      "    struct Model : Concept { Quoter quoter_; /* forwards both calls */ };",
      "    std::unique_ptr<Concept> model_;",
      "};",
    ],
    "Four unrelated quoters sit in one std::vector<AnyRateQuoter>, each dispatching through Concept.",
    { fontSize: 10 });
  addNotes(s,
    "Walk the shape: Concept is the erased interface, Model<Quoter> is a template that adapts exactly " +
    "one concrete type to it by holding it as a member and forwarding every call, and the unique_ptr<" +
    "Concept> is the only data member a caller ever sees. The requirement a type has to satisfy is a " +
    "named method shape rather than a call-operator shape, which is the dividing line from std::" +
    "function: reach for std::function when the varying thing is any callable with a signature, write " +
    "a concept/model pair by hand when it is any type with a named operation. Mention the copy " +
    "question: AnyRateQuoter needs a hand-written copy constructor because unique_ptr is move-only, not " +
    "because Concept is an incomplete type the way PImpl's Impl is.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · HIDING A CONCRETE TYPE", "PImpl's firewall, and a typed rejection", "C++20 · C++23",
    [
      "// C++20: a negative amount is reported by throwing.",
      "void RateQuoteLedger::record(std::string label, Cents amount_cents) {",
      "    if (amount_cents < 0) {",
      "        throw std::invalid_argument(\"amount_cents cannot be negative\");",
      "    }",
      "    impl_->record(std::move(label), amount_cents);",
      "}",
      "",
      "// C++23: std::expected reports the same rejection as a typed value.",
      "std::expected<void, std::string>",
      "RateQuoteLedger::try_record(std::string label, Cents amount_cents) {",
      "    if (amount_cents < 0) {",
      "        return std::unexpected<std::string>(\"amount_cents cannot be negative\");",
      "    }",
      "    impl_->record(std::move(label), amount_cents);",
      "    return {};",
      "}",
    ],
    "impl_ stays exactly one pointer in both columns; the firewall, not the error path, is the point.",
    { fontSize: 10 });
  addNotes(s,
    "Keep the focus on the firewall, not the error-handling contrast, which is the smaller story here. " +
    "RateQuoteLedger needs an out-of-line destructor because std::default_delete<Impl> requires Impl's " +
    "full definition, a hand-written copy constructor that deep-clones Impl rather than copying the " +
    "pointer, and defaulted move members because transferring a pointer never requires seeing what it " +
    "points to. The sharp edge worth flagging: unique_ptr does not propagate constness, so a const " +
    "method of RateQuoteLedger can still call a non-const method on *impl_ with no compiler complaint; " +
    "that discipline is a convention the type system does not enforce.");
}
{
  const s = S();
  addContentTitle(s, "IDIOMS · DEFERRED EVALUATION", "One pass instead of five throwaway objects");
  addBullets(s, [
    "A naive weighted sum, 0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three, builds and discards a fresh object at every operator: five throwaway objects to produce one answer.",
    "An expression template defers that arithmetic into a small tree of nodes that only describe the operation, computing nothing until the whole chain is assigned to a real variable.",
    "The claim is proven by a count, not a visual read of the code: the lazy path constructs exactly one result object; the eager path constructs five.",
    "A self-registering prototype registry removes the one remaining central file: each concrete type registers itself from its own translation unit, discoverable by a string key with nothing naming it centrally.",
  ], { fontSize: 17 });
  addNotes(s,
    "The domain is a freight dispatcher closing out a route report, weighting distance, fuel, and dwell " +
    "time across several legs. The cost this idiom removes has nothing to do with the arithmetic " +
    "itself; it is the intermediate objects an ordinary operator overload builds and discards on the " +
    "way to one final answer. Land on the lifetime rule before moving on: an expression-template node's " +
    "only safe home is inside the single statement that builds a concrete result from it, because the " +
    "nodes hold references, not copies, and naming an intermediate expression with auto and reusing it " +
    "later is the classic way this idiom turns dangerous.");
}
{
  const s = S();
  addDiagramSlide(s, "IDIOMS · DEFERRED EVALUATION", "A lazy expression tree versus eager evaluation",
    "23-expression-templates",
    "The expression template evaluates once into one result; the eager version builds a new object per operator.");
  addNotes(s,
    "Five nodes, three scaled legs and two sums, each a handful of bytes on the stack with no " +
    "arithmetic run inside any of them. The only place a full result object gets built is the " +
    "converting constructor that receives the finished tree, and it asks the root for each field value " +
    "once, which recursively asks every node beneath it, all within a single call. The test suite " +
    "behind this proves the construction count directly: one for the lazy path, five for the eager " +
    "path reaching the identical numeric answer, which is the entire argument visible as a number " +
    "rather than inferred from reading the operator definitions.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · DEFERRED EVALUATION", "Deferring arithmetic into a node, not a result", "C++20",
    [
      "template <typename Left, typename Right>",
      "class RouteSum : public RouteExpr<RouteSum<Left, Right>> {",
      "public:",
      "    RouteSum(const Left& l, const Right& r) : lhs_(l), rhs_(r) {}",
      "    double operator[](std::size_t i) const { return lhs_[i] + rhs_[i]; }",
      "private:",
      "    const Left& lhs_;",
      "    const Right& rhs_;",
      "};",
      "",
      "template <typename Expr>",
      "RouteScaled<Expr> operator*(double factor, const RouteExpr<Expr>& expr) {",
      "    return RouteScaled<Expr>(factor, expr.derived());",
      "}",
      "// 0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three builds a 5-node",
      "// tree; RouteMetrics's converting constructor evaluates it once,",
      "// in a single pass over the finished tree.",
    ],
    "Both node types hold references, never copies; that is what keeps every node cheap to build.",
    { fontSize: 10 });
  addNotes(s,
    "Neither node type does any arithmetic in its constructor; both hold references to whatever they " +
    "were built from and defer the actual multiply or add to operator[], run once per field, only when " +
    "something asks for a value. operator+ and operator* build these nodes instead of a concrete " +
    "RouteMetrics. This is the idiom behind std::valarray and every linear-algebra library that uses " +
    "expression templates: it pays for itself once the type being combined is expensive enough to copy " +
    "or allocate, and the combining expression runs often enough for that saving to matter. A three-" +
    "double struct sits at the edge of that bar; it is here because the mechanism is easiest to see " +
    "against a small type, not because three doubles is where a real codebase should reach for this.");
}
{
  const s = S();
  addDiagramSlide(s, "IDIOMS · SELF-REGISTRATION", "Each type registers itself; no central file lists them",
    "23-prototype-registry",
    "Every carrier policy's own translation unit registers its prototype at static initialization.");
  addNotes(s,
    "Compare this to a hand-maintained registry, where some caller has to remember to call register() " +
    "for every new kind and a reviewer has to remember to check that call site every time a type is " +
    "added. Here, a registrar object's constructor runs during static initialization and the " +
    "registration is a side effect of simply linking that translation unit in; nothing outside the file " +
    "ever names the concrete class. Flag the real operational risk, not just the mechanism: a linker " +
    "building from a static archive can drop an object file it sees no live symbol reference into, " +
    "registrar and all, so this idiom needs an OBJECT library, not a STATIC one, the moment it crosses " +
    "a library boundary.");
}
{
  const s = S();
  addCodeSlide(s, "IDIOMS · SELF-REGISTRATION", "A registrar that runs at static initialization", "C++20",
    [
      "template <typename Concrete>",
      "class CarrierPolicyRegistrar {",
      "public:",
      "    explicit CarrierPolicyRegistrar(std::string key) {",
      "        CarrierPolicyRegistry::instance()",
      "            .register_prototype(std::move(key), std::make_unique<Concrete>());",
      "    }",
      "};",
      "",
      "// weight_tiered_carrier_policy.cpp, its own translation unit:",
      "namespace {",
      "class WeightTieredCarrierPolicy : public CarrierPolicy { /* ... */ };",
      "const CarrierPolicyRegistrar<WeightTieredCarrierPolicy>",
      "    registrar{\"weight-tiered\"};",
      "}",
      "// No file anywhere names WeightTieredCarrierPolicy to resolve",
      "// this key later.",
    ],
    "Adding a fourth carrier means a fourth .cpp file with this same pattern; nothing else changes.",
    { fontSize: 10 });
  addNotes(s,
    "A test proves this is real, not an accident of link order, by resolving the key purely through " +
    "the registry with no include anywhere naming the concrete class. The idiom earns its cost for a " +
    "closed, growing set of kinds that all need the same treatment through the same mechanism; a " +
    "registry with one concrete type, or no second type ever coming, is paying genericity's complexity " +
    "for no benefit a direct function call would not give more plainly. This is also a clean answer to " +
    "the question a reviewer should always ask of generated registration code: does the registration " +
    "call live inside the concrete type's own file, or did it land in a central main() that reintroduces " +
    "the list this idiom exists to remove.");
}

// ============================================================================
// SECTION 02: PATTERNS, EXPRESSED IN C++
// ============================================================================
divider("02", "Patterns, expressed in C++",
  "The classic catalogue, rewritten in the idioms this deck just built.",
  "This section is the payoff: five familiar patterns, each shown in the C++ idiom that actually " +
  "ships in this codebase, including the specific place C++20 and C++23 diverge. Strategy gets two " +
  "runtime-swappable expressions, a virtual interface and a value-semantic callable. State moves to " +
  "std::variant and std::visit for a closed set checked exhaustively at compile time. Command's queue " +
  "picks up std::move_only_function in C++23 to drop a shared_ptr workaround. Factory's lookup gains " +
  "std::expected alongside its throwing form. Builder gets a plain companion: designated initializers " +
  "for the case that never needed a fluent chain at all.");

{
  const s = S();
  addContentTitle(s, "PATTERNS · OVERVIEW", "Same forces, idiomatic C++ resolutions");
  addBullets(s, [
    "Strategy: an algorithm that varies independently of the code around it, resolved as a virtual interface, a std::function value, or a concept-constrained compile-time policy.",
    "State: behavior that depends on a changing status, resolved as a GoF hierarchy, a transition table, or a closed std::variant dispatched with std::visit.",
    "Command: a request turned into an object with its own execute and undo, queued and batched independently of the call that created it.",
    "Factory Method and Abstract Factory: client code that needs an object, or a matching family of objects, without naming the concrete class that produces it.",
    "Builder: a product with required fields, optional fields, and a cross-field invariant that only a single, centralized check can enforce.",
  ], { fontSize: 16 });
  addNotes(s,
    "This slide is the map for the rest of the section. Every pattern here is the one the audience " +
    "already knows from the Gang of Four catalogue; what is new is which concrete C++ construct " +
    "resolves each force in this codebase, and in several cases, which construct C++23 adds on top of " +
    "the C++20 baseline without changing the pattern's shape. Keep returning to one discipline across " +
    "every pattern in this section: name the plain alternative, a direct function call, a bare enum, a " +
    "single constructor, next to the pattern, and let the forces actually present decide between them, " +
    "not the pattern's familiarity.");
}
{
  const s = S();
  addDiagramSlide(s, "PATTERNS · STRATEGY", "Three resolutions of the same force",
    "08-strategy-forces",
    "Runtime polymorphism behind a unique_ptr, a value-semantic std::function, and a compile-time policy.");
  addNotes(s,
    "A checkout context holds exactly one interchangeable shipping-cost strategy at a time, and this " +
    "diagram is the map of the three ways this codebase expresses that: an abstract RateStrategy behind " +
    "a unique_ptr for runtime swappability with polymorphic ownership, a std::function value for the " +
    "same swappability with no base class and the bonus that two strategies compose into a third with " +
    "no decorator class, and a concept-constrained template parameter for a pricing engine compiled " +
    "once per region with zero indirection at the call site. None of the three is the canonical Strategy " +
    "in this book's catalogue; choosing among them is itself the design decision.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · STRATEGY", "Runtime interface versus a composable callable", "C++20",
    [
      "// Runtime polymorphism: an abstract interface behind a unique_ptr.",
      "class RateStrategy {",
      "public:",
      "    virtual Cents quote(const Shipment& s) const = 0;",
      "};",
      "estimator.use_strategy(std::make_unique<TieredZoneCarrier>(/* ... */));",
      "",
      "// A value-semantic callable: any Cents(const Shipment&) target.",
      "using RateFn = std::function<Cents(const Shipment&)>;",
      "RateFn with_insurance_surcharge(RateFn base_rate, double cents_per_dollar) {",
      "    return [base_rate = std::move(base_rate), cents_per_dollar](const Shipment& s) {",
      "        return base_rate(s) + surcharge_for(s, cents_per_dollar);",
      "    };",
      "}",
    ],
    "Composing two std::function strategies needs no decorator class; composing two RateStrategy objects does.",
    { fontSize: 10 });
  addNotes(s,
    "Lead with the composition story, it is the real reason this deck treats std::function as a " +
    "distinct idiom rather than a stylistic variant of the interface version. with_insurance_surcharge " +
    "takes an existing RateFn and returns a new one that wraps it, which a std::function does for free " +
    "because it is a value closing over whatever state it needs. Getting the identical effect from the " +
    "interface version means introducing a Decorator class, a second pattern, just to wrap one " +
    "RateStrategy around another. Flag the common LLM reflex here: a model trained on tutorials reaches " +
    "for the abstract base class by default because it is the version that appears most often, whether " +
    "or not the forces call for polymorphic ownership across module boundaries.");
}
{
  const s = S();
  addDiagramSlide(s, "PATTERNS · STATE", "One rule set, four representations",
    "10-fsm-representations",
    "The same shipment transition rules as a GoF hierarchy, a transition table, and a std::variant.");
  addNotes(s,
    "A shipment status moves through a small, fixed set of states, created, paid, in transit, " +
    "delivered, cancelled, and every representation here encodes the identical legal moves " +
    "independently, which is what makes a cross-check test meaningful: three different representations " +
    "proving they agree only means something if each one states the rules itself. The GoF hierarchy " +
    "puts the rules inside the types; a transition table puts them in one literal a product manager can " +
    "scan without opening a class; std::variant with std::visit gets the type-per-status clarity with " +
    "no base class, no heap allocation, and a closed set the compiler can verify is exhaustively handled.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · STATE", "A closed set, dispatched with std::visit", "C++20",
    [
      "struct CreatedTag {}; struct PaidTag {}; struct InTransitTag {};",
      "struct DeliveredTag {}; struct CancelledTag {};",
      "using ShipmentVariant =",
      "    std::variant<CreatedTag, PaidTag, InTransitTag, DeliveredTag, CancelledTag>;",
      "",
      "ShipmentVariant apply_event(const ShipmentVariant& state, ShipmentEvent event) {",
      "    return std::visit(Overloaded{",
      "        [event](CreatedTag) -> ShipmentVariant {",
      "            if (event == ShipmentEvent::Pay) return PaidTag{};",
      "            if (event == ShipmentEvent::Cancel) return CancelledTag{};",
      "            throw IllegalTransition(ShipmentStatus::Created, event);",
      "        },",
      "        // PaidTag, InTransitTag, ... follow the same shape",
      "    }, state);",
      "}",
    ],
    "No vtable, no heap allocation: the variant's alternative list is the entire set of legal statuses.",
    { fontSize: 10 });
  addNotes(s,
    "The Overloaded set is the standard overloaded-lambda idiom: one callable per tag type, so std::" +
    "visit picks the right one at compile time instead of a chain of if constexpr. The guarantee worth " +
    "naming is the closed set: a GoF ShipmentState subclass can be added anywhere a program includes " +
    "the header, which is exactly the open extensibility a plugin wants and exactly what a reviewer " +
    "checking whether five statuses are really the only five cannot rule out by reading one file. " +
    "ShipmentVariant's alternative list is that file, in its entirety, and an exhaustive Overloaded set " +
    "will not compile if a new tag is added to the variant without a matching lambda, turning a missed " +
    "case into a build failure instead of a silent default.");
}
{
  const s = S();
  addDiagramSlide(s, "PATTERNS · COMMAND", "A queue draining into an invoker, a history moving between two stacks",
    "11-command-undo",
    "An invoker drains a FIFO queue; a history moves a command between a done stack and an undone stack.");
  addNotes(s,
    "A warehouse editor's operations, receive stock, move stock, correct a quantity, cancel an order " +
    "line, all become command objects sharing one execute-and-undo interface, which is what lets them " +
    "sit on a stack waiting to reverse, or in a queue waiting to run later, in a way a bare function " +
    "call never could. Two undo strategies appear in this one domain: a symmetric operation like moving " +
    "stock undoes by running the identical operation with the sign flipped, while setting a quantity " +
    "after a cycle count has no arithmetic inverse at all and has to capture the prior value at execute " +
    "time, not at construction time, because a queued command might run an hour after it was built.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · COMMAND", "A queued task: a shared_ptr tax, or none at all", "C++20 · C++23",
    [
      "// C++20: packaged_task is move-only; std::function needs a",
      "// copyable target, so a shared_ptr wrapper is the usual",
      "// workaround, at the cost of one extra heap allocation.",
      "auto shared_task = std::make_shared<std::packaged_task<RenderedLabel()>>(",
      "    std::move(packaged));",
      "Task task = [shared_task] { (*shared_task)(); };",
      "",
      "// C++23: std::move_only_function stores the packaged_task",
      "// directly. No extra heap allocation, no reference count to",
      "// maintain.",
      "using Task = std::move_only_function<void()>;",
      "Task task(std::move(packaged));",
    ],
    "Every production thread or command queue written against C++20 pays this tax or hand-rolls the fix.",
    { fontSize: 11 });
  addNotes(s,
    "This is not a cosmetic difference, it is worth saying plainly. std::function requires its target " +
    "to be copy-constructible even when a queue slot never actually copies the callable; std::" +
    "packaged_task is move-only by design because a packaged task owns a promise that should not be " +
    "duplicated. The C++20 workaround wraps the move-only task in a shared_ptr purely to satisfy std::" +
    "function's copy requirement, paying one heap allocation and a reference count per submitted task. " +
    "std::move_only_function exists specifically to remove that workaround. The same contrast reappears " +
    "later for a thread pool's task queue; it is the identical fix solving the identical underlying " +
    "problem in a second queue shape.");
}
{
  const s = S();
  addDiagramSlide(s, "PATTERNS · FACTORY", "A fixed sequence around one varying hook",
    "12-factory-method-flow",
    "onboard() runs a fixed sequence; sandbox and production onboarders each fill the one varying hook.");
  addNotes(s,
    "Factory Method solves a narrower problem than Abstract Factory, and this diagram is the clean " +
    "version of that distinction: onboard() validates an account, runs a test suite, and judges the " +
    "result, and that sequence never changes, while create_test_suite() is a private virtual hook that " +
    "decides which one product gets built. Compare this to Abstract Factory's job: a single factory " +
    "call producing a matching family of three related products, a label formatter, a rate quoter, a " +
    "tracking client, so nothing downstream can end up holding a mismatched set from two different " +
    "carriers. The test worth repeating: Factory Method varies one product inside a fixed sequence; " +
    "Abstract Factory varies a whole family a caller asks for in one call.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · FACTORY", "An unrecognized name: thrown, or a typed value", "C++20 · C++23",
    [
      "// C++20: an unrecognized carrier name is reported by throwing.",
      "std::unique_ptr<CarrierIntegrationFactory>",
      "make_carrier_factory(std::string_view name, std::string account_id) {",
      "    if (name == \"fastship\")",
      "        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));",
      "    throw std::invalid_argument(\"unknown carrier name\");",
      "}",
      "",
      "// C++23: std::expected reports the same failure as a typed value.",
      "std::expected<std::unique_ptr<CarrierIntegrationFactory>, std::string>",
      "try_make_carrier_factory(std::string_view name, std::string account_id) {",
      "    if (name == \"fastship\")",
      "        return std::make_unique<FastShipIntegrationFactory>(std::move(account_id));",
      "    return std::unexpected<std::string>(\"unknown carrier name\");",
      "}",
    ],
    "Both resolve the identical names to the identical concrete factories; only the failure path moves.",
    { fontSize: 10 });
  addNotes(s,
    "Both functions resolve the exact same carrier names to the exact same concrete factories, and " +
    "ShipmentProcessor's own code never names either concrete factory class regardless of which lookup " +
    "produced it. The C++23 version is additive, not a replacement: a caller on a request path that " +
    "cannot tolerate an unhandled exception reaches for try_make_carrier_factory, while existing " +
    "callers keep the throwing form. Tie this back to the AI-sidebar habit worth naming in review: a " +
    "generated fix for a new carrier should add a new concrete factory class, leaving this lookup " +
    "function's existing branches untouched, not thread a new concrete type's name into code that is " +
    "supposed to depend only on the abstraction.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · BUILDER", "Validating once, at the single point of assembly", "C++20",
    [
      "class ManifestBuilder {",
      "public:",
      "    ManifestBuilder& carrier(std::string name);",
      "    ManifestBuilder& destination(std::string name);",
      "    ManifestBuilder& add_package(std::unique_ptr<PackageTemplate> pkg);",
      "    ShipmentManifest build();  // validates once, here",
      "private:",
      "    ShipmentManifest manifest_;",
      "};",
      "",
      "ShipmentManifest built = ManifestBuilder()",
      "    .shipment_id(\"SHIP-4471\")",
      "    .carrier(\"Northline Freight\")",
      "    .destination(\"Columbus, OH\")",
      "    .add_package(registry.create(\"fragile-electronics\"))",
      "    .build();",
    ],
    "Every setter returns *this; build() is the one place the required-field invariant is checked.",
    { fontSize: 10 });
  addNotes(s,
    "Every chained setter returns a reference to the same builder, which is what lets a caller supply " +
    "fields in whatever order is convenient rather than a fixed constructor parameter list, and nothing " +
    "is validated as the chain runs. The discipline worth repeating from the AI-sidebar habit: a common " +
    "generated Builder validates inside every setter instead of once at build(), because fail-fast " +
    "sounds like good advice in isolation, but that reintroduces the fixed-ordering problem Builder " +
    "exists to remove, just spread across several methods instead of one constructor. build() moves the " +
    "finished manifest out and resets the builder's internal state in the same statement, so one " +
    "builder instance is reusable for a second manifest immediately.");
}
{
  const s = S();
  addCodeSlide(s, "PATTERNS · BUILDER", "When there is nothing left to validate", "C++20 aggregates",
    [
      "// A request with two required fields and no cross-field",
      "// invariant does not need a fluent builder; a designated",
      "// initializer names every field at the call site directly.",
      "struct DropoffRequest {",
      "    std::string carrier;",
      "    std::string destination;",
      "    ShippingPriority priority = ShippingPriority::Standard;",
      "};",
      "",
      "DropoffRequest request{",
      "    .carrier = \"Northline Freight\",",
      "    .destination = \"Columbus, OH\",",
      "};",
      "// No chained calls, no build() step: there is nothing left to",
      "// validate across fields, so the type itself is the whole",
      "// contract.",
    ],
    "Designated initializers have shipped since C++20; the choice here is Builder's cost, not the standard.",
    { fontSize: 10 });
  addNotes(s,
    "This is the when-not-to-use-it slide made concrete in code rather than left as prose. " +
    "Designated initializers are a C++20 feature already, so the contrast here is not between " +
    "standards, it is between a type that earns Builder's ceremony and one that does not: two required " +
    "fields, one optional field with a sensible default, and no cross-field invariant to enforce at a " +
    "single build() call. Introducing ManifestBuilder's full machinery for a shape this plain would add " +
    "a class, a chain of methods a reader has to trace back to one build() call, and a runtime " +
    "validation check standing in for what the type system already enforces directly through required " +
    "constructor-like fields.");
}

// ============================================================================
// SECTION 03: IN THE WILD
// ============================================================================
divider("03", "In the wild",
  "Resource pools, concurrency shapes, and dispatch shortcuts production C++ reaches for daily.",
  "This section leaves the Gang of Four catalogue for patterns that never made the original 1994 " +
  "book but show up in almost every production C++ service: a bounded resource pool with an RAII " +
  "lease, the sibling concurrency shapes a thread pool, a monitor object, and an active object each " +
  "answer, and three lightweight dispatch shortcuts that stand in for a full Strategy, State, or " +
  "Command hierarchy when the behavior behind each branch is small. Close by naming exactly " +
  "where each shortcut stops being enough and the full pattern from the previous section takes back " +
  "over.");

{
  const s = S();
  addContentTitle(s, "IN THE WILD · RESOURCE POOLS", "One RAII lease shape, four production answers");
  addBullets(s, [
    "A connection pool bounds how many expensive-to-open connections exist at once; a Lease handle returns its resource to the pool on every exit path, with no release() method to forget.",
    "A thread pool drains a shared task queue across several worker threads: many unrelated jobs, one shared pool of workers.",
    "A monitor object owns its own mutex and locks it at the top of every public method, so a caller can never forget to lock before touching shared state.",
    "An active object decouples invocation from execution entirely: calling the method only enqueues the work and returns a future; the caller never blocks.",
  ], { fontSize: 17 });
  addNotes(s,
    "All four share one concern, bounding or serializing access to something expensive or unsafe to " +
    "share carelessly, which is why a connection pool and an active object belong in the same " +
    "conversation even though they look nothing alike on the page. This deck treats the deep threading " +
    "mechanics, memory ordering, lock-free structures, as out of scope; the point here is the shape " +
    "each pattern gives that mechanism, what a pool, a monitor, and an active object look like as types " +
    "a caller depends on, and which of the four to reach for given a concrete scenario.");
}
{
  const s = S();
  addDiagramSlide(s, "IN THE WILD · RESOURCE POOLS", "A bounded pool and its RAII lease",
    "24-resource-pools",
    "A caller acquires through an RAII lease; the same shape underlies a connection pool, a thread pool, a monitor, and an active object.");
  addNotes(s,
    "A shipment-tracking service does not open a fresh connection per scan event; opening one costs a " +
    "DNS lookup, a handshake, often a TLS negotiation, work measured in milliseconds against a query " +
    "that takes microseconds once the connection exists. The pool builds a bounded set once and hands " +
    "leases out; the bound is what makes this more than a cache, a cache grows to fit demand, a pool's " +
    "whole purpose is capping demand at a number the backing resource can actually sustain. A test " +
    "substitutes a mock connection factory and asserts ten acquire-and-release cycles against a " +
    "capacity-two pool call the factory exactly twice, never once per acquire.");
}
{
  const s = S();
  addCodeSlide(s, "IN THE WILD · RESOURCE POOLS", "A lease with no release method to forget", "C++20",
    [
      "template <typename Resource>",
      "class ResourcePool {",
      "public:",
      "    using Factory = std::function<std::unique_ptr<Resource>()>;",
      "    class Lease {",
      "    public:",
      "        Resource& operator*() const { return *resource_; }",
      "        // move-only; destructor returns the resource to the pool",
      "    };",
      "    Lease acquire(std::chrono::milliseconds timeout);",
      "};",
      "",
      "{",
      "    auto lease = pool.acquire(100ms);",
      "    lease->record_event(\"SHM-1001\", \"in_transit\");",
      "}  // lease destructor runs here: the connection returns to the pool",
    ],
    "A Lease is move-only for the same reason a dock bay lease is: two owners of one pooled slot is a bug.",
    { fontSize: 10 });
  addNotes(s,
    "This is the RAII idiom from the opening section, applied to something with a hard bound on how " +
    "many can exist at once. Lease's destructor calls back into the pool and pushes the resource onto " +
    "the free list regardless of whether the scope exited normally, early, or through an exception " +
    "unwinding past it; a caller never writes pool.release(connection), because there is no such method " +
    "to call or to forget. Flag the common generated mistake directly: an LLM asked for a thread-safe " +
    "connection pool frequently returns a raw pointer with a separate release() method a caller must " +
    "remember on every exit path, including exception paths the generated code rarely considers, which " +
    "reintroduces exactly the leak RAII exists to close.");
}
{
  const s = S();
  addDiagramSlide(s, "IN THE WILD · CONCURRENCY SHAPES", "Shared queue, many workers, versus one private queue",
    "24-threadpool-vs-active-object",
    "A thread pool's shared queue serves concurrent workers; an active object's private queue serves one dedicated worker.");
  addNotes(s,
    "The clearest way to tell these two shapes apart on sight: a thread pool is for many different, " +
    "unrelated jobs sharing a pool of workers, CPU-bound work like rendering a shipping label's barcode " +
    "that several requests want done concurrently. An active object is for one narrow request type that " +
    "needs its own private, serialized timeline, printing a label on a physical printer that can only " +
    "run one job at a time, where a caller must never block on submission. The active object's queue " +
    "skips type erasure entirely, because every request it accepts shares the exact same signature, no " +
    "Task wrapper, no C++20/23 fork needed there at all.");
}
{
  const s = S();
  addCodeSlide(s, "IN THE WILD · CONCURRENCY SHAPES", "The identical shared_ptr tax, in a thread pool's queue", "C++20 · C++23",
    [
      "// C++20: the same shared_ptr workaround Command's queue needed,",
      "// now for a thread pool's task queue.",
      "using Task = std::function<void()>;",
      "auto shared_task = std::make_shared<std::packaged_task<RenderedLabel()>>(",
      "    std::move(packaged));",
      "Task task = [shared_task] { (*shared_task)(); };",
      "",
      "// C++23: std::move_only_function removes the shared_ptr tax",
      "// entirely.",
      "using Task = std::move_only_function<void()>;",
      "Task task(std::move(packaged));",
    ],
    "Both presets build and pass the identical AllSubmittedTasksRunExactlyOnce test either way.",
    { fontSize: 11 });
  addNotes(s,
    "Worth naming out loud that this is the same fix, in the same shape, as the Command queue's task " +
    "type earlier in this deck; a std::move_only_function-storing queue is now the default answer " +
    "anywhere a move-only packaged task needs to sit behind a type-erased callable. The worker loop " +
    "pulling from this queue uses std::jthread and std::stop_token together so no hand-kept stop flag " +
    "is needed: the condition variable's stop-token overload wakes every waiting worker the instant the " +
    "pool's destructor requests a stop, with no manual notify_all anywhere in this code.");
}
{
  const s = S();
  addCodeSlide(s, "IN THE WILD · CONCURRENCY SHAPES", "Serialize access, or decouple invocation from execution", "C++20",
    [
      "// Monitor object: every public method takes the object's own lock.",
      "void ShipmentManifestMonitor::add_item(std::string sku, double kg) {",
      "    std::lock_guard lock(mutex_);",
      "    items_.emplace_back(std::move(sku), kg);",
      "}  // runs on the caller's thread, one caller at a time",
      "",
      "// Active object: invoking the method only enqueues the work.",
      "std::future<PrintResult> print_label(ShippingLabelRequest request) {",
      "    std::packaged_task<PrintResult()> task(/* renders request */);",
      "    std::future<PrintResult> future = task.get_future();",
      "    { std::lock_guard lock(mutex_); requests_.push_back(std::move(task)); }",
      "    cv_.notify_one();",
      "    return future;  // caller never blocks on the printer",
      "}",
    ],
    "A monitor still runs work on the caller's thread; an active object moves the work to its own.",
    { fontSize: 10 });
  addNotes(s,
    "The distinguishing question between these two: a monitor object answers how several threads touch " +
    "the same mutable state safely, several warehouse scan threads adding items to one manifest " +
    "concurrently, and it still runs every call on the caller's own thread, one caller at a time. An " +
    "active object answers a different question, what happens when a caller wants to say print this " +
    "and move on immediately while the actual work happens on its own timeline. A test asserts " +
    "print_label itself returns in under fifty milliseconds regardless of how long the render takes, " +
    "and a second test compares the executing thread's id against the caller's to prove the work really " +
    "moved, not merely that the API looks asynchronous.");
}
{
  const s = S();
  addContentTitle(s, "IN THE WILD · DISPATCH SHORTCUTS", "Three lightweight alternatives to a full hierarchy");
  addBullets(s, [
    "A shipment-tracking webhook has to route a message-type tag to one short handler function; a full Strategy, State, or Command class per tag is often over-engineering for behavior this small.",
    "An enum-indexed jump table resolves a known, closed tag set in one array lookup, no branching, no vtable; its risk is a positional convention nothing in the type system enforces.",
    "A dispatch map over an open, runtime-extensible key set lets a new carrier register a handler after the program has already started, with no recompile.",
    "A constexpr std::array table gets the jump table's speed back and adds a static_assert that catches a missing handler at compile time instead of at a crash.",
  ], { fontSize: 17 });
  addNotes(s,
    "Name the force precisely: a dispatch table answers which function runs for a key, and nothing " +
    "more. The moment the answer needs more than a function call, the table is the wrong tool and the " +
    "full pattern it was standing in for is the right one again. A handler that needs to remember state " +
    "across calls is a Strategy problem; a handler whose outcome depends on more than the incoming " +
    "message, on a shipment's current status too, is a State problem; a handler that needs to be undone " +
    "is a Command problem. These three forms are the lightweight answer for exactly the case where none " +
    "of that is true.");
}
{
  const s = S();
  addDiagramSlide(s, "IN THE WILD · DISPATCH SHORTCUTS", "Three dispatch mechanisms, one shared set of handlers",
    "25-dispatch-tables",
    "Jump table and constexpr table on the closed side, the open dispatch map on the right, sharing one set of handlers.");
  addNotes(s,
    "All three forms route to the identical six handler functions, which is deliberate: if the routing " +
    "mechanism changes, the behavior behind each route should not, and the only way to make that true " +
    "by construction rather than by care is to call the same functions from every mechanism. The jump " +
    "table and the constexpr table serve a closed enum this codebase compiles against directly; the " +
    "dispatch map serves a wire tag arriving from a third-party carrier webhook, a string this codebase " +
    "does not control and cannot enumerate ahead of time, which is exactly why it needs to stay open at " +
    "runtime rather than closed at compile time.");
}
{
  const s = S();
  addCodeSlide(s, "IN THE WILD · DISPATCH SHORTCUTS", "A table checked complete, at compile time", "C++20",
    [
      "constexpr std::array<MessageHandler, kCarrierMessageTypeCount>",
      "make_dispatch_table() {",
      "    std::array<MessageHandler, kCarrierMessageTypeCount> table{};",
      "    table[message_type_index(CarrierMessageType::Created)] = &handle_created;",
      "    table[message_type_index(CarrierMessageType::Delivered)] = &handle_delivered;",
      "    // ... one line per enumerator, in any order",
      "    return table;",
      "}",
      "inline constexpr auto kDispatchTable = make_dispatch_table();",
      "",
      "static_assert(std::ranges::none_of(kDispatchTable,",
      "    [](MessageHandler h) { return h == nullptr; }),",
      "    \"every CarrierMessageType must have a handler entry\");",
    ],
    "A forgotten line here is a null entry, and the static_assert turns that into a build failure.",
    { fontSize: 10 });
  addNotes(s,
    "Compare this to a bare jump table's weak point: a plain array indexed by enum position has to " +
    "stay in the exact declaration order of the enum, and inserting a new enumerator in the middle " +
    "silently shifts every handler after it with no compiler warning. Here, every assignment names the " +
    "enumerator it is filling a slot for, so the order lines are written in no longer matters, and " +
    "std::array default-initializes unfilled slots to nullptr, which is what lets a static_assert walk " +
    "the finished table before the program exists at all and fail the build on a forgotten handler " +
    "instead of crashing the first time that message type arrives in production.");
}
{
  const s = S();
  addCodeSlide(s, "IN THE WILD · DISPATCH SHORTCUTS", "An open, runtime-registered table, and its contiguous form", "C++20 · C++23",
    [
      "// C++20: std::unordered_map. Correct, but node-based: a heap",
      "// allocation per entry and a hash computation per lookup.",
      "std::unordered_map<std::string, Handler> handlers_;",
      "",
      "void register_handler(std::string wire_tag, Handler handler) {",
      "    handlers_[std::move(wire_tag)] = std::move(handler);",
      "}",
      "// A caller registers \"CUSTOMS_HOLD\" after construction, at",
      "// runtime; the very next dispatch() call routes to it.",
      "",
      "// C++23: std::flat_map stores keys and values in two contiguous,",
      "// sorted vectors, better cache behavior for a read-often table.",
      "std::flat_map<std::string, Handler> handlers_;",
    ],
    "register_handler() and dispatch() are unmodified; only the one member's type moved forward.",
    { fontSize: 10 });
  addNotes(s,
    "register_handler is the entire point of choosing a map over a fixed array: a caller reaches it " +
    "after construction, while the program is already running, and the mechanism's behavior changes " +
    "with no recompile. This dispatch map's profile is write-rarely, read-often, a handful of handlers " +
    "registered once at startup or onboarding, dispatched on every single inbound message afterward, " +
    "which is exactly the profile std::flat_map targets: trading an amortized hash lookup for a binary " +
    "search over memory a CPU can actually prefetch. An unrecognized wire tag always falls back to a " +
    "named handler rather than crashing, which matters more here than for the closed forms, because a " +
    "string key carries no guarantee the dispatcher has ever seen it before.");
}

// ============================================================================
// CLOSING
// ============================================================================
{
  const s = S();
  addContentTitle(s, "CLOSING", "The C++20/23 toolkit, in one pass");
  addTwoColBullets(s,
    [
      { text: "The substrate", options: { bullet: false, bold: true } },
      "RAII ties lifetime to scope; unique_ptr, shared_ptr, and weak_ptr make an object graph's ownership explicit in the type.",
      { text: "Compile-time expression", options: { bullet: false, bold: true } },
      "CRTP, policy-based design, and expression templates resolve polymorphism and arithmetic with zero runtime dispatch.",
      { text: "Hiding a concrete type", options: { bullet: false, bold: true } },
      "Type erasure for types you do not control; PImpl for a stable public layout over a changing implementation.",
    ],
    [
      { text: "C++23's additions", options: { bullet: false, bold: true } },
      "std::expected for typed rejection, std::move_only_function to drop a shared_ptr tax, deducing this, std::out_ptr, std::flat_map, std::print.",
      { text: "The constant underneath", options: { bullet: false, bold: true } },
      "Every refinement is additive: the pattern's shape, and its test suite, hold steady across both standards.",
      { text: "The discipline", options: { bullet: false, bold: true } },
      "Name the plain alternative next to the idiom, and let a measured force decide, not the idiom's novelty.",
    ],
    { fontSize: 15 });
  addNotes(s,
    "Close by tying the whole deck back to one habit: every C++23 feature shown here was additive to a " +
    "C++20 baseline that already worked, a typed std::expected alongside a throwing path, std::" +
    "move_only_function removing a shared_ptr workaround, std::out_ptr narrowing an unmanaged pointer's " +
    "window, deducing this removing a CRTP template argument. None of them changed what a pattern does; " +
    "all of them removed a small, specific cost or ambiguity a C++20 caller had to accept. The bigger " +
    "habit worth leaving the room with is the one repeated under every pattern and idiom in this deck: " +
    "name the plain, non-generic alternative next to the sophisticated one, and let a real force, a " +
    "second implementation, a measured cost, a concurrent-access bug, decide between them.");
}

// ============================================================================
// APPENDIX
// ============================================================================
divider("Appendix", "Reference material",
  "The build toolchain and a one-page C++20-to-C++23 feature map. Keep this open in another window.",
  "This appendix is reference material, not a narrative arc; it is meant to stay open in a second " +
  "window while working through the rest of the deck's examples, not to be read front to back. The " +
  "first slide covers the local build-and-test loop every example in the source book follows; the " +
  "second is a single-page map of every C++20-to-C++23 upgrade this deck showed, for a quick lookup " +
  "when a reviewer asks which standard a given refinement belongs to.");

{
  const s = S();
  addDiagramSlide(s, "APPENDIX · BUILD AND TOOLCHAIN", "The local build-and-test loop",
    "00-build-and-test-loop",
    "Edit, conan install, cmake --preset, clang build, ctest verify, on the host, no containers.");
  addNotes(s,
    "Every example behind this deck builds the same way: a pinned local toolchain, clang as the " +
    "default compiler with CMake 3.28 or newer, Ninja as the generator, and Conan 2.x supplying " +
    "GoogleTest 1.17.0 including GoogleMock. There is no container anywhere in the build or test loop, " +
    "and no example ever asks a reader to run one; a fedora:44 job container shows up only as CI " +
    "substrate, never as something a chapter tells a reader to reach for locally. Two presets cover the " +
    "whole deck, cpp23 as the default build and cpp20 as the compatibility build, with every C++23-only " +
    "snippet guarded behind a preprocessor check so the cpp20 preset always compiles clean. A C++26 " +
    "preset is not present yet; its headline features are still prerelease across " +
    "compilers.");
}
{
  const s = S();
  addContentTitle(s, "APPENDIX · FEATURE MAP", "C++20 baseline versus the C++23 refinement");
  addStatusTable(s, [
    { code: "Resource acquisition failure", name: "Throws (BayUnavailableError)", purpose: "std::expected typed result" },
    { code: "Adopting a C factory's pointer", name: "Manual two-step unique_ptr", purpose: "std::out_ptr / std::inout_ptr" },
    { code: "Self-type dispatch", name: "CRTP (template + static_cast)", purpose: "Deducing this (explicit object param)" },
    { code: "Move-only callable in a queue", name: "std::function + shared_ptr tax", purpose: "std::move_only_function" },
    { code: "Open dispatch table storage", name: "std::unordered_map (node-based)", purpose: "std::flat_map (contiguous)" },
    { code: "Enum-to-index conversion", name: "static_cast<size_t>(enum)", purpose: "std::to_underlying(enum)" },
    { code: "Formatted terminal output", name: "std::format + std::cout", purpose: "std::print / std::println" },
    { code: "Typed rejection at a lookup", name: "optional / throw, reason discarded", purpose: "std::expected<T, Reason>" },
  ], { colW: [4.1, 4.0, 3.99], rowH: 0.56 });
  addNotes(s,
    "Use this as the lookup slide, not a teaching slide: every row names the exact mechanism shown " +
    "earlier in this deck, with the C++20 baseline on the left and the C++23 refinement on the right. " +
    "The pattern worth repeating one last time is visible across every single row: the C++20 column " +
    "always compiles and always ships correct behavior on its own, and the C++23 column never replaces " +
    "it, it only adds a typed error, removes an allocation, or narrows an unsafe window. A team on a " +
    "C++20-only toolchain loses none of the patterns or idioms in this deck; they only give up these " +
    "eight specific, local refinements until they can move to C++23.");
}

pres.writeFile({ fileName: OUT })
  .then(p => console.log("WROTE", p))
  .catch(e => { console.error(e); process.exit(1); });
