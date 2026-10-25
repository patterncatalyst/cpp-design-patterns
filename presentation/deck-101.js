// deck-101.js - "Design Patterns with C++" - 101: Patterns & SOLID
// Red Hat-branded 16:9 deck. Build:  node deck-101.js  (from presentation/)

"use strict";

const H = require("./deck-helpers.js");
const {
  COLOR, FONT, W, ASSETS, PNG,
  newDeck, addFooter, addContentTitle, addBullets,
  addStatusTable, addCaption, addSectionDivider, addNotes,
} = H;

const OUT = "cpp-design-patterns-101-patterns-and-solid.pptx";

const pres = newDeck();
pres.title = "Design Patterns with C++ - 101: Patterns & SOLID";
let pageNum = 0;

function S() {
  const s = pres.addSlide(); pageNum += 1; addFooter(s, pageNum); return s;
}
function divider(code, title, subtitle, notes) {
  const s = pres.addSlide(); pageNum += 1;
  addSectionDivider(s, code, title, subtitle);
  addNotes(s, notes);
}

// A pattern slide: title + 2-4 bullets, optional figure on the right.
function patternSlide(eyebrow, title, bullets, fig, caption, notes) {
  const s = S();
  addContentTitle(s, eyebrow, title);
  const hasFig = !!fig;
  addBullets(s, bullets, {
    x: 0.62, y: 1.85, w: hasFig ? 5.55 : 12.09, h: 4.55,
    fontSize: hasFig ? 15.5 : 17,
  });
  if (hasFig) {
    s.addImage({
      path: `${PNG}/${fig}.png`,
      x: 6.45, y: 1.75, w: 6.25, h: 4.35,
      sizing: { type: "contain", w: 6.25, h: 4.35 },
    });
  }
  if (caption) addCaption(s, caption);
  addNotes(s, notes);
  return s;
}

// A pattern-to-force reference table slide.
function refTable(eyebrow, title, rows, notes) {
  const s = S();
  addContentTitle(s, eyebrow, title);
  addStatusTable(s, rows, { colW: [2.60, 1.80, 7.69], rowH: 0.42 });
  addNotes(s, notes);
}

// ===========================================================================
// Cover
// ===========================================================================
{
  const s = pres.addSlide();
  pageNum += 1;
  s.background = { color: COLOR.white };
  try { s.addImage({ path: `${ASSETS}/cover-panel.png`, x: 0, y: 0, w: W, h: 7.5 }); } catch (e) {}
  s.addText("DESIGN PATTERNS WITH C++", {
    x: 6.00, y: 1.98, w: 6.90, h: 0.34,
    fontFace: FONT.title, fontSize: 14, bold: true, color: COLOR.red, charSpacing: 6,
    align: "left", valign: "middle",
  });
  s.addText([
    { text: "Design Patterns", options: { breakLine: true } },
    { text: "with C++" },
  ], {
    x: 5.95, y: 2.42, w: 6.95, h: 2.00,
    fontFace: FONT.title, fontSize: 48, bold: true, color: COLOR.ink,
    align: "left", valign: "top",
  });
  s.addText("101: Patterns & SOLID", {
    x: 6.00, y: 4.65, w: 6.70, h: 0.90,
    fontFace: FONT.body, fontSize: 18, italic: true, color: COLOR.caption,
    align: "left", valign: "top",
  });
  try { s.addImage({ path: `${ASSETS}/logo-candidate-2.png`, x: 11.10, y: 6.80, w: 1.55, h: 0.37 }); } catch (e) {}
  addNotes(s, "Design Patterns with C++, the 101 deck: patterns and SOLID. This is the conceptual half of the series. The question running through every slide is not what the code looks like, but what pressure in the design made someone reach for a name. We will walk the SOLID principles, then the Gang of Four catalogue grouped by intent, then two lightweight tools for when a full pattern is more than the problem needs, and close with a quick-reference appendix worth keeping open in a second window. Every example lives in a shipping and logistics domain: carriers, rates, manifests, warehouses. The deep C++ implementation detail is a separate deck; this one is about recognizing what to build, and why.");
}

// ===========================================================================
// Section: Why Patterns
// ===========================================================================
divider("01", "Why Patterns", "Recognizing a decision under forces",
  "This section sets up the vocabulary everything else depends on. A pattern is not a recipe to copy from memory; it is a name attached to a recurring set of forces, competing requirements that pull against each other in the same piece of code. Naming a pattern before naming the forces behind it is how a design decision turns into decoration. We will also look at why that same discipline matters more, not less, once an AI pairing partner can produce any pattern's shape on request.");

patternSlide(
  "WHY PATTERNS",
  "Patterns as decisions under forces",
  [
    "A pattern is a name for a recurring trade-off, not a recipe to copy into new code; the same name can resolve into different C++ depending on which forces are actually present.",
    "A force is a requirement that competes with another requirement in the same code: flexibility against simplicity, compile-time cost against runtime cost, testability against directness.",
    "The recurring arc: recognize the forces, name a candidate pattern and a candidate plain alternative, choose the idiomatic expression, test the behavior, and know when to stop.",
  ],
  "01-forces-and-decisions",
  "Recognize the forces, name a candidate pattern or non-pattern, choose the idiomatic C++ expression, test it, and know when not to use it. Forces and trade-offs run under every step.",
  "Picture an engineer who just finished the Gang of Four catalogue and reaches for Strategy the next time a function has an if statement. The result compiles, demonstrates real knowledge of the pattern's shape, and is worse than what it replaced, because the name arrived before the question of what problem it was supposed to solve. A pattern's name is worth using because an underlying tension recurs across different code in different domains, and the name lets two engineers skip re-deriving that tension from scratch. The arc on this slide runs under every pattern we cover today: forces first, name second, idiomatic expression third, a test that proves the claimed behavior fourth, and a clear-eyed read on when the pattern stops paying for itself."
);

patternSlide(
  "WHY PATTERNS",
  "A shared vocabulary for teams and AI",
  [
    "A pattern name compresses a paragraph of trade-off reasoning into a word a reviewer can act on: this needs to be a Strategy, not a switch, because partner tiers are now owned by separate teams.",
    "The same compression applies to an AI pairing partner: hand it the forces, not just the keyword, and it can make the same idiomatic call a human engineer would.",
    "Watch for a virtual interface standing in for a choice that never needed runtime polymorphism: one production implementation, and a second that exists only to prove the interface is pluggable.",
  ],
  "02-shared-vocabulary",
  "A pattern name sits between three readers, a teammate, a future engineer, and an AI assistant, compressing a cluster of forces and a chosen structure into one word all three can act on the same way.",
  "A pattern name only does useful work when everyone means the same thing by it, and that is a harder property to maintain than it sounds. A model trained on a huge amount of code has absorbed just as much code that reaches for a pattern's name loosely as code that reaches for it correctly. Prompting with forces, not just a keyword, is how you keep a generated diff accurate: ask for a pluggable discount calculator and you risk an abstract base class with three subclasses and a factory for a decision that is fixed at compile time with no second case coming. The review question is identical whether a human or a model wrote the diff: what forces does this solve, and was a plainer alternative considered and rejected for a stated reason."
);

// ===========================================================================
// Section: SOLID
// ===========================================================================
divider("02", "SOLID", "Five principles, one force: this will change",
  "SOLID is not a separate topic from patterns; it is the forces-and-trade-offs vocabulary the whole catalogue gets judged against. All five principles respond to one underlying pressure: the code around a decision should not have to change just because the decision itself changes. We will walk a billing engine through Single Responsibility, Open/Closed, and Liskov Substitution, then an order-notification system through Interface Segregation, Dependency Inversion, and dependency injection without a framework.");

{
  const s = S();
  addContentTitle(s, "SOLID", "Single Responsibility");
  addBullets(s, [
    "A billing engine's one Invoice class doing three jobs, totaling, formatting a receipt, and persisting the record, means a tax fix, a wording change, and a storage change all land in the same file.",
    "SRP splits by reason to change, not by line count: a calculator, a formatter, and a repository, each with exactly one reason to change and no knowledge the other two exist.",
    "Skip the split when nothing has a second reason to change yet; a class with one job and no second job in sight does not need to be carved into pieces in advance.",
  ], { x: 0.62, y: 1.85, w: 5.55, h: 4.55, fontSize: 15.5 });
  s.addImage({ path: `${PNG}/03-solid-before-after.png`, x: 6.45, y: 1.75, w: 6.25, h: 4.35, sizing: { type: "contain", w: 6.25, h: 4.35 } });
  addCaption(s, "A billing engine before and after SOLID: one Invoice type doing three jobs, split by SRP into three focused types, extended by OCP through a rule abstraction, and refined by LSP without breaking the base contract.");
  addNotes(s, "The naive version is one Invoice class with calculate_total, format_receipt, and save methods, each reaching into the same line items and each changing for its own unrelated reason. A reviewer approving a tax fix has to read past formatting code and persistence code to find the three lines that matter. SRP's fix is not making the class smaller for its own sake; it is separating three unrelated reasons to change that were sharing one address. None of the three resulting types knows the other two exist, which is what makes changing the tax calculation a single-file edit instead of a careful read of a tangled class.");
}

{
  const s = S();
  addContentTitle(s, "SOLID", "Open/Closed");
  addBullets(s, [
    "A tax rate changing twice a year is predictable, and the naive fix is an if branch added to a function every time a jurisdiction or a discount program launches.",
    "A rule abstraction lets a new tax or discount rule arrive as a new type the calculator depends on but never edits; adding a rule touches exactly one new file.",
    "Skip the abstraction when one rule is the only one in hand or coming; an interface with a single implementation is a layer with nothing underneath it yet.",
  ], { fontSize: 17 });
  addNotes(s, "Open to new behavior, closed against editing what already works and is already tested. A percentage tax rule and a percentage discount rule both existed before a flat service fee rule joined them, and adding the flat fee touched exactly one new file; the calculator's totals_for method is byte for byte what it was before. The strongest proof of this is a test that defines a brand-new rule type the production header has never seen and hands it to the calculator's constructor with zero production code changed, because the abstraction was always the real extension point, not a convention someone had to remember.");
}

{
  const s = S();
  addContentTitle(s, "SOLID", "Liskov Substitution");
  addBullets(s, [
    "A subtype must be usable anywhere its base type is, and that promise covers the whole contract, not just a matching method signature.",
    "A loyalty discount rule refines a percentage discount rule by overriding only a protected rate hook; the base method owns a clamp the subtype can never bypass, so a combined rate can never discount more than the invoice is worth.",
    "Skip the protected hook when only one subtype will ever override it; a hook with one caller is a promise of flexibility nobody is going to collect on.",
  ], { x: 0.62, y: 1.85, w: 5.55, h: 4.55, fontSize: 15.5 });
  s.addImage({ path: `${PNG}/03-lsp-clamp.png`, x: 6.45, y: 1.75, w: 6.25, h: 4.35, sizing: { type: "contain", w: 6.25, h: 4.35 } });
  addCaption(s, "A subtotal flows through the overridable discount_rate() hook into a clamp the base method owns and never exposes, so every subtype's rate is bounded before it reaches the subtotal.");
  addNotes(s, "A loyalty customer with a steep negotiated base rate and decades on the account would, unclamped, produce a combined rate past one hundred percent, discounting more than the invoice is worth. The subtype never has to defend against that itself, because the clamp runs on whatever the rate hook returns, including a value the subtype never anticipated. The substitution claim gets proven with one function written once against the base class reference, called with both the base rule and the refined rule, behaving correctly both times because neither subtype had the chance to violate the invariant the base class owns.");
}

{
  const s = S();
  addContentTitle(s, "SOLID", "Interface Segregation");
  addBullets(s, [
    "A fat NotificationGateway interface forces an SMS channel to implement a template renderer and a delivery-status check, capabilities it does not have, and it ends up throwing or faking a silent default for both.",
    "Segregating into a message sender, a template renderer, and a delivery-status provider means the SMS channel only ever declares the one capability it has, with no method to stub and no caller that can call one by mistake.",
    "Skip further segregation once every remaining method is used by every remaining client; splitting a complete interface into one-method types is churn dressed up as principle.",
  ], { fontSize: 17 });
  addNotes(s, "The fat interface manufactures a Liskov violation that segregation makes structurally impossible, because there is nothing left to stub. A static assertion can even prove the absence at compile time: the SMS channel's type is asserted to not inherit from the template-renderer role, so if someone later adds the capability back without updating the type, the build fails before a caller gets the chance to call a method that was never meant to exist on that channel. Segregation also changes who has to depend on what: an auditor that only ever checks delivery status has no business depending on the sender or renderer roles at all, and its own test double now implements exactly one method instead of three.");
}

{
  const s = S();
  addContentTitle(s, "SOLID", "Dependency Inversion & DI by hand");
  addBullets(s, [
    "An order notification policy that reaches straight into a concrete email gateway can never be tested without a real or stubbed email client, and it cannot add an SMS channel without an edit.",
    "Dependency inversion routes both the policy and the gateway toward one abstraction, a message sender, so the policy depends on what a channel does, never which channel it is; a composition root wires the concrete gateways once, by hand, with no container.",
    "Reach for a DI container only once the object graph is large enough that hand-wiring becomes its own burden; a graph wired once at startup reads better as a few lines in main than as a registration framework.",
  ], { x: 0.62, y: 1.85, w: 5.55, h: 4.55, fontSize: 15.5 });
  s.addImage({ path: `${PNG}/04-solid-dependency-inversion.png`, x: 6.45, y: 1.75, w: 6.25, h: 4.35, sizing: { type: "contain", w: 6.25, h: 4.35 } });
  addCaption(s, "Dependency inversion routes a high-level policy through an abstraction instead of straight to a concrete gateway, inverting the dependency arrow; interface segregation splits that abstraction into role interfaces a concrete type implements only where it has the capability.");
  addNotes(s, "C++ ships no reflection and no runtime container that scans a project and wires constructors together, and the idiomatic answer is not to build one. A composition root, usually main or a thin wrapper around it, constructs every concrete type exactly once and passes each abstraction's concrete implementation to whatever constructor asks for it. A reference, not a smart pointer, is the right ownership choice when the composition root and every consumer share one lifetime, which is the common case for a dependency wired once at startup; reach for a smart pointer when a dependency's lifetime has to be managed independently, the way a runtime-swappable strategy does.");
}

// ===========================================================================
// Section: The GoF catalogue by intent - Creational
// ===========================================================================
divider("03", "Creational Patterns", "The GoF catalogue by intent, part 1 of 3",
  "Creational patterns answer one family of forces: how an object gets built when the decision of which concrete type, or how many related objects, is not fixed at the call site. Four patterns, one recurring question each: who decides the concrete type, how does a multi-field object get assembled safely, how does a configured exemplar get reused, and when does exactly one instance actually need to be global.");

patternSlide(
  "CREATIONAL · GOF CATALOGUE",
  "Factory Method",
  [
    "A carrier onboarding workflow runs the same validate-then-test sequence for every account, but which integration suite to run depends on whether the account is sandbox or production.",
    "Factory Method puts that one decision behind a method a subclass overrides, so the fixed sequence around it never has to branch on environment.",
    "Skip it when the workflow only has one product to build; a single hook with one implementation is a base class with nothing to protect.",
  ],
  "12-factory-method-flow",
  "The onboarding workflow runs a fixed sequence around one varying hook, which the sandbox and production onboarders each fill with a different test suite, one product decided by environment.",
  "The workflow itself, validate the account, run a suite, judge the result, never changes. Only the one step that decides which suite to instantiate does, and that decision belongs to a subclass, not an if statement the workflow would otherwise have to carry. The moment to introduce this as a factory method is the moment a second environment, not a second format of the same thing, needs a different product at that one point in an otherwise identical sequence. One base class, one override, standing in for a single function with no decision to make, is the sign this pattern arrived before the forces did."
);

patternSlide(
  "CREATIONAL · GOF CATALOGUE",
  "Abstract Factory",
  [
    "A shipping label, a rate quote, and a tracking URL all have to come from the same carrier, or the label on the box will not match the bill.",
    "Abstract Factory hands out that whole matching family from one call, so checkout code can never end up holding a mismatched set.",
    "Skip it when only one carrier exists; a one-implementation family interface is three abstractions guarding against a mix-up that cannot happen.",
  ],
  "12-abstract-factory-families",
  "An abstract factory declares a family of three abstract products; each carrier's own factory implements and creates its own matching family, and the client depends only on the abstractions.",
  "Mixing a label from one carrier with a rate quote from another is not a smaller bug; it is a shipment a warehouse worker cannot scan, billed by a formula that does not match the label on the box. A single factory call produces every member of a related family at once, so nothing downstream of that call can end up holding a mismatched set. Watch for the inverted trap too: a three-method interface where two of the three methods have exactly one caller and no second implementation anywhere is the Abstract Factory shape being used to solve a narrower Factory Method problem."
);

patternSlide(
  "CREATIONAL · GOF CATALOGUE",
  "Builder & Prototype",
  [
    "A freight manifest has required fields, optional fields with sane defaults, and one rule that only makes sense once every field is set: it needs at least one package.",
    "Builder assembles that manifest through a fluent chain and checks the cross-field rule exactly once, at build time, instead of in a constructor nobody can read.",
    "A warehouse that reuses the same few package kinds clones a configured prototype instead of re-deriving fragile-electronics or bulk-dry-goods defaults at every call site.",
    "Skip Builder below two or three plain fields with no shared rule; skip Prototype when there is nothing common left to clone.",
  ],
  "13-builder-prototype",
  "A builder assembles a manifest behind a fluent interface and validates it once at build; a prototype registry clones a named template into an independent copy; serialize and deserialize round-trip the result through plain text.",
  "A single constructor cannot express required fields, optional fields with defaults, and a cross-field invariant all at once without becoming unreadable or forcing every caller to repeat the same defaults. Builder's value concentrates in that one validation step; a design with no meaningful cross-field rule to centralize has already given most of that value away before paying any of the cost. Prototype's registry pays for itself once a handful of pre-configured kinds get reused often enough that re-deriving their defaults at every call site is real, visible duplication, and the round trip through serialize and deserialize is what proves the clone, and the builder's defaults, were complete, not just a read of the source code that looks right."
);

patternSlide(
  "CREATIONAL · GOF CATALOGUE",
  "Singleton versus dependency injection",
  [
    "A shipment audit log wants one call site reachable from anywhere a fulfillment event happens, which is exactly Singleton's pitch.",
    "The same capability, injected as a reference through a composition root, gives every caller the one shared log without a global, and without a test needing to fight the global to fake it.",
    "Reach for a true singleton only when nothing will ever need to substitute a second instance; reach for injection the moment a test, or a second implementation, shows up.",
  ],
  "14-singleton-vs-di",
  "A singleton's clients reach into one global instance directly; the DI alternative routes the same capability through an abstraction a composition root wires, swappable with a test double.",
  "A process-wide, stateless resource with no plausible second implementation and nothing mutable worth racing on, a read-only table of constants loaded once at startup, is a case where Singleton's cost is close to zero, because there is nothing to substitute. A shipment audit log is mutated by every call, read by audits, and depended on by more than one component, which is exactly the profile where a test needs to substitute a double and a reviewer needs to see the dependency in a constructor signature. The question is never whether Singleton is wrong; it is whether a force the pattern resolves is actually present, or whether a plain injected object is already simpler."
);

// ===========================================================================
// Section: The GoF catalogue by intent - Structural
// ===========================================================================
divider("04", "Structural Patterns", "The GoF catalogue by intent, part 2 of 3",
  "Structural patterns are not about varying an algorithm or producing a family of objects; they are about shape. How an existing, awkward interface gets one the rest of the code can use. How a client gets one call instead of several to orchestrate. How two things that each vary on their own get decoupled so adding one does not force a change to the other. How a class hides what is likely to change behind a stable header.");

patternSlide(
  "STRUCTURAL · GOF CATALOGUE",
  "Adapter",
  [
    "A carrier's rate API predates this codebase, with its own units and its own method names, and calling code should not have to match them.",
    "Adapter wraps the legacy call behind the interface the rest of the system already expects, translating units once instead of at every call site.",
    "Skip it when the legacy call already matches the target shape; wrapping a method that needs no translation adds a class around nothing.",
  ],
  "15-adapter-facade-bridge",
  "Adapter wraps a legacy API behind a target interface; Facade fronts three subsystems with one call; Bridge crosses an abstraction axis of notification kinds with an implementation axis of transports.",
  "None of these three structural forces are about varying an algorithm or producing a family of related objects; they are about shape. Adapter specifically needs an actual mismatch to resolve: if the legacy rate API already took a shipment and returned a cost in the right unit, wrapping it would add a class and a virtual call around a method that could be called directly. The adapter in this example is justified because the legacy API's units and names differ from what the rest of the codebase expects, not because wrapping things is good practice in general."
);

{
  const s = S();
  addContentTitle(s, "STRUCTURAL · GOF CATALOGUE", "Facade");
  addBullets(s, [
    "Quoting and labeling a shipment is three calls that always have to run together, in order, or a label ships with no matching rate behind it.",
    "Facade gives checkout code one call that owns that sequence, so no caller can skip a step or run the calls out of order.",
    "Skip it when there is only one subsystem to call; a method that only forwards to a single call is indirection with a different name.",
  ], { fontSize: 17 });
  addNotes(s, "The signal that a facade is pulling its weight is the sequencing logic inside the one call it exposes, not the number of constructor parameters it happens to take. A class with three constructor parameters and no coordination rule between the calls it makes is closer to a parameter struct than to this pattern. A facade is worth its layer only when more than one subsystem needs a real sequencing or coordination rule enforced between them, the way quote-then-label-then-register-tracking does here.");
}

patternSlide(
  "STRUCTURAL · GOF CATALOGUE",
  "Bridge",
  [
    "A notification can be a shipment-created event or a delayed event, and it can travel by email or by SMS, two axes of variation that should not multiply into one class per combination.",
    "Bridge splits the notification kind from its transport into two small hierarchies, so a new kind and a new transport combine without a new class for every pairing.",
    "Skip it when only one kind ever rides one transport; splitting an axis that never varies independently buys nothing.",
  ],
  "15-bridge-matrix",
  "Two notification kinds crossed with two transports give four live combinations from two small hierarchies, not the class-per-combination explosion a single hierarchy would need.",
  "Bridge needs two axes that vary independently in practice, not one axis dressed up as two. If every notification in this example only ever went out by email, splitting the kind from the transport would buy nothing: there would be exactly one combination, achievable with one class and no bridge at all. The moment to introduce the split is the moment a second kind or a second transport is either in hand or clearly coming, the same test that governs Abstract Factory's family of products."
);

{
  const s = S();
  addContentTitle(s, "STRUCTURAL · GOF CATALOGUE", "PImpl");
  addBullets(s, [
    "A label renderer's formatting logic changes often, and every file that only calls render should not have to recompile every time it does.",
    "PImpl hides that implementation behind a pointer, so the header stays stable while the formatting detail changes underneath it.",
    "Skip it when the class is small and stable; the hand-written destructor and copy operations PImpl costs only pay back against real compile-time churn.",
  ], { fontSize: 17 });
  addNotes(s, "PImpl answers a narrower question than the other three structural patterns: not what an object's public shape should be, but how to keep that shape stable while its private implementation changes underneath it. The idiom costs real code, a hand-written destructor, copy constructor, and copy assignment operator, in exchange for a compilation firewall. That trade only pays off when the implementation detail is large, likely to change, or pulls in a dependency the rest of the codebase should not have to include transitively, which is exactly the profile a formatting engine has and a small, stable class does not.");
}

patternSlide(
  "STRUCTURAL · GOF CATALOGUE",
  "Decorator",
  [
    "A shipping rate quote wants a high-value surcharge, a cache so a cart priced three times does not hit the tariff table three times, and an audit log, each shipped on its own schedule.",
    "Decorator stacks each concern behind the same interface the rate source implements, so any combination still looks like one quote to the checkout flow asking for it.",
    "Skip it when a rate source has exactly one concern; wrapping a single-purpose object that nothing else will ever stack alongside adds a hop with nothing behind it.",
  ],
  "16-decorator-proxy",
  "A decorator stack of surcharge, caching, and logging wraps a core rate source, next to a proxy controlling access to a real manifest store; both wrap the same interface their subject implements, only the intent differs.",
  "Order matters in a decorator stack in a way that is easy to miss: reordering a cache layer and a surcharge layer changes which total a repeat call returns, because a cache hit short-circuits before the layer beneath it runs again. Decorator and Proxy can look nearly identical in code, which is exactly why the test for telling them apart is never the shape; it is what each one is for. A decorator composes independent concerns a caller assembles in whatever order it needs; a proxy controls construction timing or access to one subject and nothing else stacks alongside it."
);

{
  const s = S();
  addContentTitle(s, "STRUCTURAL · GOF CATALOGUE", "Proxy");
  addBullets(s, [
    "A warehouse scanning app's manifest archive is expensive to open, and a guest account has no business reading what a worker's badge unlocks.",
    "Proxy stands in for the real archive behind the identical interface, controlling when it gets built or who can reach it, never what a successful call returns.",
    "Skip it when construction is cheap and every caller has the same access; a proxy guarding nothing measured is defending against a cost that was never counted.",
  ], { fontSize: 17 });
  addNotes(s, "A surrogate that implements the exact interface the real subject implements, distinguishable from the real thing only by the one behavior it exists to change, construction timing or access control. The sharper trap is building a caching proxy when the problem actually calls for a caching decorator: a cache meant to compose with other concerns on the same object belongs in a stack a caller assembles, while a cache whose entire job is gating whether an expensive call happens at all, with no other concern sharing that wrapped object, is closer to what proxy's intent describes.");
}

patternSlide(
  "STRUCTURAL · GOF CATALOGUE",
  "Composite",
  [
    "A pallet holds cartons, a carton can hold more cartons before it holds one item, and how much does this weigh should mean the same call at every level.",
    "Composite gives the leaf and the thing that holds leaves one shared interface, so weighing a pallet is the same call as weighing the item inside it.",
    "Skip it when nesting never happens; a flat list of items summed in a loop says the same thing with one interface fewer.",
  ],
  "17-composite-flyweight",
  "A composite tree of a pallet, cartons, and items shares one interface on the left; a flyweight pool of SKU descriptors referenced by many line items sits on the right.",
  "The naive answer writes one function to weigh an item and a nearly identical one to weigh a carton, and the moment a carton gets nested inside another carton, which happens the first time a warehouse consolidates two partial shipments into one box, the carton function has to learn to call itself. Composite refuses that duplication outright: one interface, implemented by both the leaf and the thing that holds leaves, so weighing means the same call no matter how deep the tree goes. It justifies its interface only when a tree can actually get deep; a flat list of items with no nesting on any roadmap does not need it."
);

{
  const s = S();
  addContentTitle(s, "STRUCTURAL · GOF CATALOGUE", "Flyweight & Null Object");
  addBullets(s, [
    "One SKU can sit on hundreds of line items a day, and storing its full description on every line item copies facts that never change between one line and the next.",
    "Flyweight shares the immutable part, the SKU descriptor, across every line item that references it, so a thousand line items reference one description instead of carrying a thousand copies.",
    "A weight-limit violation needs somewhere to report to, and most of the time nobody has wired up a real handler yet; Null Object stands in with a do-nothing implementation instead of a null check at every call site.",
    "Skip Flyweight below a few dozen references; skip Null Object when an absent collaborator is a bug, not a normal case, because that case needs a loud failure, not a silent one.",
  ], { fontSize: 17 });
  addNotes(s, "Flyweight starts paying for itself once reference counts climb into the hundreds or thousands per shared value, the point at which a warehouse running real volume actually sits, and where the memory a naive copy-per-reference design would spend becomes visible on a profiler rather than theoretical. Null Object's test is whether absence is a valid state of the world the rest of the program should tolerate, or a bug that should be caught as close to its source as possible; reaching for it to paper over a collaborator that should never legitimately be missing, a database connection a service cannot run without, trades a loud early failure for a silent one.");
}

// ===========================================================================
// Section: The GoF catalogue by intent - Behavioral
// ===========================================================================
divider("05", "Behavioral Patterns", "The GoF catalogue by intent, part 3 of 3",
  "Behavioral patterns coordinate decisions, sequences, requests, and state. This is the largest group in the catalogue, eleven patterns, because behavior is where most of a system's day-to-day change pressure actually lands: a rule that varies, a status that governs what is legal, a request that needs to outlive the call that created it, a set of collaborators that should not know about each other.");

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Strategy",
  [
    "A checkout flow quotes a shipping cost by a rule that is not fixed: flat rate, by weight, or a tiered table, chosen per carrier contract, not per line of code.",
    "Strategy extracts the pricing rule into its own swappable unit, runtime polymorphism, a std::function value, or a compile-time policy, so checkout code depends on what the rule does, not which one it is.",
    "Skip it when one rule is the only one in hand or coming; a branch that reads start to finish for one team is the whole story already.",
  ],
  "08-strategy-forces",
  "A checkout context holds one interchangeable shipping-cost strategy at a time, expressed as runtime polymorphism, a value-semantic std::function, or a compile-time concept policy, trading runtime flexibility against zero overhead.",
  "What makes Strategy worth singling out is that modern C++ gives three distinct ways to resolve the same force, each with its own cost and its own guarantee. An interface behind a unique_ptr buys runtime swappability at the price of a virtual call and a heap allocation. A std::function buys the same swappability as a value, with the bonus that two strategies compose into a third without a decorator class. A template parameter constrained by a concept buys the same quote with no indirection at all, at the price of fixing the choice before the program runs. None of the three is the pattern; all three are, and choosing among them is itself the design decision."
);

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Template Method",
  [
    "Exporting an order to three downstream systems runs the same five-step sequence every time, validate, header, lines, footer, trailer, and only a few of those steps change per format.",
    "Template Method fixes the sequence in one base method and lets each format override only the steps that vary, with the non-virtual-interface idiom keeping the sequence itself from being overridden.",
    "Skip it when only one format exists; a base class protecting a sequence from variation that nobody has introduced yet is overhead with nothing to guard.",
  ],
  "09-template-method-skeleton",
  "An order-export pipeline runs a fixed sequence and calls private virtual hooks in a fixed order; concrete exporters override the hooks without touching the sequence or the validate and trailer invariants.",
  "A generated version of this pattern, described without more context, usually makes the entry point itself virtual and lets a subclass override it directly, which throws away the one guarantee Template Method exists to provide. The non-virtual-interface idiom closes that gap with two rules: the public entry point is never virtual, so no subclass can touch the sequence, and the steps that vary are private virtual methods called only from inside the base class, so a subclass supplies their content but has no way to call them directly. Strategy swaps an entire algorithm for another of the same shape; this pattern keeps one algorithm and varies a handful of its steps while the order never moves."
);

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "State",
  [
    "A shipment moves through a fixed set of statuses, created, paid, in transit, delivered, and the real question is which events are legal right now, not which pricing rule applies.",
    "State puts the legal-transition rules inside the current status itself, so a reviewer reading the paid state sees what a paid shipment can do next without tracing an if chain.",
    "Skip it when a status has two values and one transition; a bool or a plain if says the same thing with no hierarchy to navigate.",
  ],
  "10-state-transitions",
  "A shipment's legal states and transitions, plus the weighted-probability outlet a stochastic variant of the same machine draws from out of in-transit.",
  "A data-driven transition table puts the same rules in one literal structure instead, so checking whether a shipment can be cancelled after it ships is reading one row, or its absence, rather than tracing a virtual call; a variant dispatched with visit gets the type-per-status clarity with no base class, no unique_ptr, and no heap allocation anywhere in the machine. All of these earn their cost when a thing has more than a couple of statuses and illegal events are a real case the code must guard against; a status with two values and one transition needs a bool, not a hierarchy."
);

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Command",
  [
    "A warehouse editor's operations need to be undoable, queued for a nightly batch, or submitted from more than one trigger without each one re-deriving how to run them.",
    "Command turns a request into an object that remembers how to run itself and how to reverse itself, so it can sit in a queue or on an undo stack after the call that created it returns.",
    "Skip it when a request is triggered from one place, never undone, and always runs immediately; a plain method call is the whole story and a Command object around it is a detour.",
  ],
  "11-command-undo",
  "An invoker drains a FIFO command queue into a receiver, and a command history moves a command between a done stack and an undone stack as undo and redo run.",
  "Turning a request into an object instead of a function call is what makes undo, batching, and multiple submitters possible at all: a bare function call has no existence after it returns, while a command object does, for as long as something is holding onto it. Not every queued action needs the full ceremony of an object with an execute and an undo; a plain callable serves an action with nothing to reverse, logging an event or firing a webhook, at a fraction of the cost. The forces decide which of the two shapes an action gets, not which one is more familiar."
);

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Observer",
  [
    "A shipment's status change matters to a support dashboard, an alerting service, and a billing dispute log, each one reacting on its own schedule, added independently of the others.",
    "Observer lets the shipment tracker broadcast one fact, its state changed, to an open set of listeners it never has to name, import, or even know exist.",
    "Skip it when exactly one piece of code ever reacts to the change; a direct call at the point of change is plainer than a subscription pipeline with one subscriber.",
  ],
  "18-observer-mediator",
  "A subject fans a status change out to independent observers, next to a mediator routing between collaborators that never reference each other directly, shown against the tangled direct mesh it replaces.",
  "Observer frees a subject from needing to know who is listening to it; nothing about an observer's identity ever reaches the subject's own code. A status change with exactly one piece of code that reacts to it does not need a subscription pipeline at all; wrapping a single, fixed reaction before a second listener is in hand or clearly coming adds an allocation, a subscription to manage, and a reader's detour through a class definition to find the one call site that will ever use it."
);

{
  const s = S();
  addContentTitle(s, "BEHAVIORAL · GOF CATALOGUE", "Mediator");
  addBullets(s, [
    "A warehouse, a carrier, and a notification service calling each other directly means a fourth collaborator, returns processing, requires editing every class it needs to hear from.",
    "Mediator routes every message through one coordinator instead, so no collaborator ever calls another one's method directly and a new one only has to talk to the hub.",
    "Skip it when a handful of collaborators never grows past that; a plain, visible sequence of calls is the whole story and a hub adds a layer to trace through for no reason yet.",
  ], { fontSize: 17 });
  addNotes(s, "Mediator frees a set of peers from knowing about each other at all, and pays for that with a hub that does know about every one of them; that trade-off, not how similar the code looks, is what separates it from Observer. Three components that only ever grow by one and never gain a fourth collaborator do not need a hub standing between them. The point where the calculus flips is more than a handful of collaborators, different teams owning different ones, or a routing rule that changes independently of any single collaborator's own logic.");
}

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Chain of Responsibility",
  [
    "A shipment approval needs a weight check, a hazmat clearance check, and an embargo check, each owned by a different team, changing on its own schedule, and the submitting code should not know how many checks exist.",
    "Chain of Responsibility passes the request down a line of handlers that each act on it or hand it off, so the sender holds nothing more than a reference to the first handler.",
    "Skip it when two or three checks are fixed, owned by one team, and never reordered; an if chain inside one function reads start to finish just as well.",
  ],
  "19-chain-iterator",
  "A request flows down a chain of handlers that each handle or pass, next to an aggregate traversed by a hand-written forward iterator and by a filter-and-transform ranges pipeline over the same data.",
  "This pattern coordinates a sequence of decisions, in contrast to Iterator which coordinates a sequence of elements, and reading them together builds the habit of naming the actual force at work before reaching for either shape. The point where a chain of handler objects starts paying for itself is an open-ended number of checks, ownership split across more than one team, or a need to change which checks run and in what order without touching a shared function; a fixed set of two or three checks owned by one team is still just a branch."
);

{
  const s = S();
  addContentTitle(s, "BEHAVIORAL · GOF CATALOGUE", "Iterator");
  addBullets(s, [
    "A warehouse manifest sorts packages into one lane per destination zone internally, and a caller that wants to walk every package in zone order should not have to know it is bucketed.",
    "Iterator presents a uniform, sequential view over that real structure, and once it satisfies the standard library's concepts the same aggregate becomes a range a views pipeline can filter and transform.",
    "Skip it when storage is already a flat container; writing a custom iterator around a vector that already provides sequential access adds a forwarding layer with nothing to hide.",
  ], { fontSize: 17 });
  addNotes(s, "The reason a custom iterator is justified in this example is specifically that the manifest's real storage, four zone buckets, is not what a caller should see; reach for a custom iterator only when there is a real structure like that to hide. A ranges pipeline with views::filter and views::transform is worth composing when a caller needs a declarative subset-and-project view over a range, not as a way to dress up a single pass a plain for loop already expresses clearly.");
}

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Visitor",
  [
    "A merchant-defined shipping filter is a small, fixed grammar, but what a team wants to do with it, evaluate it, render it, eventually price it, keeps growing.",
    "Visitor adds a new operation over that fixed rule structure without reopening every node class, through classic double dispatch or a std::variant with std::visit.",
    "Skip it when one operation runs in one place; the branches a single function can hold already say the whole story.",
  ],
  "20-visitor-memento-interpreter",
  "A shared rule-node tree walked two ways, a classic double-dispatch visitor and a std::variant plus std::visit visitor producing the same result, with an interpreter walking the same tree to a grammar result.",
  "Visitor pays for itself when a structure stays fixed and the operations over it multiply; a rule tree with exactly one operation, evaluated in exactly one place, needs neither shape, just the handful of branches a single function can hold. Introducing either expression ahead of a second operation becoming a real requirement is the same trap as a premature Decorator: a vtable or a template instantiation paid for before anything needs it. The two idiomatic expressions trade the same way Strategy's three do, dispatch mechanism against compile-time structure, not correctness against correctness."
);

patternSlide(
  "BEHAVIORAL · GOF CATALOGUE",
  "Memento",
  [
    "Drafting a filter in an admin screen sometimes needs to throw away everything since an earlier step and start over, a whole-state rollback, not one reversed operation.",
    "Memento captures everything an editor is, including internals nobody outside it should see, into an opaque token a caretaker can hold and hand back later.",
    "Skip it when there is no multi-step editing workflow to roll back; an object mutated directly with no undo path costs nothing until somebody actually asks for one.",
  ],
  "20-memento-save-restore",
  "The editor saves its whole state into a snapshot a caretaker can hold and return but cannot read, and restore swaps that state back in a single call.",
  "The sharper trap with this pattern is reaching for it when Command's per-operation undo is what the problem actually calls for: cloning an entire object on every edit to support undoing one keystroke, when a single cheap inverse operation would do the same job for a fraction of the cost. Name which rollback shape a caller needs, one step back or one whole batch discarded, before building either pattern's machinery. A caretaker holding the snapshot never gets to read what is inside it, which is what keeps the editor's internals private even while its whole state is recoverable."
);

{
  const s = S();
  addContentTitle(s, "BEHAVIORAL · GOF CATALOGUE", "Interpreter");
  addBullets(s, [
    "A merchant's shipping filter grammar, hold packages over twenty kilograms, flag international shipments over a declared value, is built from user input, not fixed at compile time.",
    "Interpreter expresses that grammar as types that evaluate themselves, answering what a whole filter expression evaluates to given how its pieces nest.",
    "Skip it when an expression is hard-coded with no editor anywhere near it; a plain boolean check written where it is used does not need a tree underneath it.",
  ], { fontSize: 17 });
  addNotes(s, "Interpreter pays for itself when a grammar is truly open-ended, built from configuration or user input rather than fixed at compile time by the handful of call sites that use it. A single hard-coded filter condition, checked once in checkout code with no merchant-facing editor anywhere near it, does not need a tree, a node hierarchy, or an evaluate method walking it; it needs a plain boolean expression written directly where it is checked. Building an interpreter for a grammar with one fixed expression and no second expression coming is solving a parsing-and-evaluation problem nobody has.");
}

// ===========================================================================
// Section: Choosing Well
// ===========================================================================
divider("06", "Choosing Well", "Lighter tools, and knowing when a pattern is not one",
  "Not every varying behavior needs a full pattern. This section covers two lightweight tools: a dispatch table as a stand-in for a hierarchy when the behavior behind each branch is small, and patternitis, the discipline of recognizing when a pattern's shape showed up because it is familiar, not because the forces called for it.");

patternSlide(
  "CHOOSING WELL",
  "Dispatch tables and maps",
  [
    "A carrier webhook feed tags every message with a type, created, picked up, delivered, and something has to run the one function that knows what to do with each tag.",
    "A jump table, a dispatch map, or a constexpr array route a tag to a handler in one lookup, without the class, constructor, and factory a full Strategy or Command would add.",
    "Reach past a table the moment a handler's behavior gets substantial enough to need its own invariants; a three-line handler does not need a hierarchy, but a stateful one might.",
  ],
  "25-dispatch-tables",
  "A shipment message type selects its handler three ways, an enum-indexed function-pointer table, an open unordered_map, and a compile-time constexpr array, each a lighter stand-in for a Strategy, State, or Command hierarchy.",
  "The least disciplined way to write this routing is an if-else chain that keeps growing sideways as message kinds are added, with every branch paying the cost of every branch before it and a reviewer's attention stretched across a function doing six unrelated things. A message-type dispatcher that calls one short function per tag and otherwise does nothing clever is a smaller problem than Strategy, State, or Command were built to solve, and reaching for a full class hierarchy to solve it is over-engineering in the other direction. A constexpr table adds one more property: it can be checked for completeness at compile time, turning a missing handler into a build failure instead of a null function pointer waiting to crash in production."
);

patternSlide(
  "CHOOSING WELL",
  "Anti-patterns and patternitis",
  [
    "Patternitis is reaching for a named pattern because its shape is familiar, not because anything in the code calls for it, and it rarely shows up as one visibly wrong line.",
    "A configuration lookup can pick up a singleton, a factory, a second singleton for the factory, and a god object tying it together, one reasonable-sounding decision at a time, to multiply two numbers.",
    "The fix is the same forces test every pattern in this catalogue gets checked against: name the pattern and the plain alternative side by side, and let the force actually present in the code decide.",
  ],
  "26-anti-patterns",
  "A singleton registry, a factory-of-factories, and a god object on one side, collapsed on the other into one service that takes a settings value by injection, which is what makes the rewrite testable in isolation.",
  "Every pattern in this catalogue is included because a real force justifies it, and that same catalogue teaches a shape an engineer can apply from memory long after the forces that justified it stopped applying, or without ever checking whether they applied in the first place. The design that over-patterns a simple multiplication needs five types and two singletons to get there, and every individual step along the way was defensible in isolation: a registry becomes a singleton because singletons are convenient to reach from anywhere, the object it hands out gets resolved through a factory because factories decouple construction from use. The simplified rewrite injects one plain value into one class and gets one value back, and that is also what makes it testable without fighting any global state."
);

// ===========================================================================
// Close
// ===========================================================================
{
  const s = S();
  addContentTitle(s, "CLOSING", "How to choose: the forces test");
  addBullets(s, [
    "Ask what varies in the code, what stays fixed, and what is expensive to get wrong, before any pattern name enters the conversation.",
    "Name a candidate pattern and a candidate plain alternative side by side, and let the forces actually present decide between them.",
    "A pattern belongs in this catalogue because a force justifies it; the same pattern belongs in your code only when that force shows up there too.",
    "The test travels: it works the same way reviewing a teammate's pull request or an AI assistant's generated diff.",
  ], { fontSize: 17 });
  addNotes(s, "Every pattern walked through today, SOLID's five principles and the Gang of Four's catalogue grouped by intent, runs through the same arc: recognize the forces, name a candidate pattern and a candidate plain alternative, choose the idiomatic C++ expression, test the behavior, and know when it stops paying for itself. A pattern name shows up often enough in interviews that it is tempting to treat the whole catalogue as trivia; the actual skill is asking what an abstraction is buying you right now, in this code, for this team, and having a real answer either way. That question does not change whether the diff in front of you was written by a teammate or generated by an assistant.");
}

// ===========================================================================
// Appendix
// ===========================================================================
divider("07", "Appendix", "Reference tables and reading list, worth a second window",
  "The following reference tables are mined directly from the pattern-to-force quick-reference ledger. Keep this section open in a second window during a design discussion: scan from the force you are facing back to the pattern name that answers it, rather than the other way around.");

refTable(
  "APPENDIX · REFERENCE",
  "Principles: SOLID",
  [
    { code: "Single Responsibility", name: "Principle", purpose: "A class has more than one reason to change, so one concern's edit risks another." },
    { code: "Open/Closed", name: "Principle", purpose: "New behavior should arrive as new code, not edits to working code." },
    { code: "Liskov Substitution", name: "Principle", purpose: "A subtype must be usable anywhere its base is, without surprising a caller." },
    { code: "Interface Segregation", name: "Principle", purpose: "A client is forced to depend on methods it never calls." },
    { code: "Dependency Inversion", name: "Principle", purpose: "High-level policy should not depend on low-level detail; both depend on an abstraction." },
  ],
  "A quick-reference scan from force to principle. Use this table the way the SOLID section used it: a competing requirement in the left-hand column names the principle that resolves it in the row. Keep this slide open as a checklist during a design review."
);

refTable(
  "APPENDIX · REFERENCE",
  "Creational patterns",
  [
    { code: "Factory Method", name: "Creational", purpose: "Which concrete type to build is a decision resolved at runtime, not at the call site." },
    { code: "Abstract Factory", name: "Creational", purpose: "A whole family of related objects must be built in a consistent variant together." },
    { code: "Builder", name: "Creational", purpose: "An object has many optional parts and construction should read as a sequence of choices." },
    { code: "Prototype", name: "Creational", purpose: "A new object is best made by copying a configured exemplar rather than rebuilding it." },
    { code: "Singleton", name: "Creational", purpose: "Exactly one instance must exist, it is effectively immutable, and nothing needs to substitute another." },
  ],
  "Five creational patterns, each mapped to the one force that justifies reaching for it. This is the same ledger used earlier for Factory Method, Abstract Factory, Builder, Prototype, and Singleton; use it as a scan-from-force lookup during a design discussion rather than re-deriving each pattern's intent from memory."
);

refTable(
  "APPENDIX · REFERENCE",
  "Structural patterns",
  [
    { code: "Adapter", name: "Structural", purpose: "An existing type has the right behavior behind the wrong interface." },
    { code: "Facade", name: "Structural", purpose: "A subsystem's many moving parts should present one small, intentional surface." },
    { code: "Bridge", name: "Structural", purpose: "An abstraction and its implementation must vary independently." },
    { code: "PImpl", name: "Structural", purpose: "A class should hide its implementation to cut compile-time coupling across a header." },
    { code: "Decorator", name: "Structural", purpose: "Behavior should be added to one object at runtime without subclassing every combination." },
    { code: "Proxy", name: "Structural", purpose: "Access to an object needs a stand-in for lazy creation, caching, or control." },
    { code: "Composite", name: "Structural", purpose: "Clients should treat a single object and a tree of objects the same way." },
    { code: "Flyweight", name: "Structural", purpose: "Many objects share most of their state and the duplication is too expensive to keep." },
    { code: "Null Object", name: "Structural", purpose: "A do-nothing case should remove a null check from every call site." },
  ],
  "Nine structural entries, covering Adapter, Facade, and Bridge as a group, PImpl as a related compilation-firewall idiom, Decorator and Proxy as the pair told apart by intent, and Composite, Flyweight, and Null Object as the trio that shares one palletizing domain. Scan down the right column for the force in front of you."
);

refTable(
  "APPENDIX · REFERENCE",
  "Behavioral patterns (1 of 2)",
  [
    { code: "Strategy", name: "Behavioral", purpose: "An algorithm chosen by a caller must be swappable without touching the caller." },
    { code: "Template Method", name: "Behavioral", purpose: "A fixed sequence has steps that vary, and the sequence itself must not." },
    { code: "State", name: "Behavioral", purpose: "Behavior depends on a current status, and illegal transitions must be refused." },
    { code: "Command", name: "Behavioral", purpose: "A request must become an object so it can be queued, logged, or undone." },
    { code: "Observer", name: "Behavioral", purpose: "Subscribers must react to a change without the subject knowing who they are." },
    { code: "Mediator", name: "Behavioral", purpose: "Many objects' direct references to each other have become a tangle." },
  ],
  "The first six of eleven behavioral entries. Strategy and Template Method both touch an algorithm's shape, one by swapping it whole, one by varying a few of its steps. State, Command, Observer, and Mediator each coordinate a different kind of change: a status, a request, a notification, or a mesh of peers."
);

refTable(
  "APPENDIX · REFERENCE",
  "Behavioral patterns (2 of 2)",
  [
    { code: "Chain of Responsibility", name: "Behavioral", purpose: "A request should pass along a line of handlers until one takes it." },
    { code: "Iterator", name: "Behavioral", purpose: "A collection's elements must be traversed without exposing its structure." },
    { code: "Visitor", name: "Behavioral", purpose: "A new operation must be added across a fixed set of types without editing each." },
    { code: "Memento", name: "Behavioral", purpose: "An object's state must be captured and restored without exposing its internals." },
    { code: "Interpreter", name: "Behavioral", purpose: "A small, recurring grammar is best expressed as types that evaluate themselves." },
  ],
  "The remaining five behavioral entries. Chain of Responsibility and Iterator are paired by shape, coordinating a sequence of decisions versus a sequence of elements. Visitor, Memento, and Interpreter all operate over the same kind of fixed tree structure, told apart by what each one is for: adding an operation, snapshotting state, or evaluating a grammar."
);

{
  const s = S();
  addContentTitle(s, "APPENDIX · FOR DEEPER COVERAGE", "Reading list");
  addBullets(s, [
    "Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, Design Patterns: Elements of Reusable Object-Oriented Software (Addison-Wesley).",
    "Robert C. Martin, Agile Software Development: Principles, Patterns, and Practices (Prentice Hall).",
  ], { fontSize: 18 });
  addNotes(s, "Two sources behind everything in this deck. The Gang of Four book is the canonical catalogue this whole pattern-to-force ledger indexes, where each pattern's intent, structure, and consequences are set out in full. Robert Martin's book is the original statement of the SOLID principles and an early, explicit treatment of patterns chosen by the forces in front of you rather than applied from memory. Both are worth reading in full once a pattern's name alone stops being enough to settle a design review.");
}

// ===========================================================================
pres.writeFile({ fileName: OUT })
  .then(p => console.log("WROTE", p, "slides:", pageNum))
  .catch(e => { console.error(e); process.exit(1); });
