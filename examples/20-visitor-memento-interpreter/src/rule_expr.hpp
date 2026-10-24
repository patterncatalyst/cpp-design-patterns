#pragma once

#include <memory>
#include <string>
#include <variant>

#include "shipping_domain.hpp"

namespace rules {

using shipping::Cents;
using shipping::DestinationZone;
using shipping::Shipment;

// The modern counterpart to classic_visitor.hpp's class hierarchy: the
// same six node kinds, now as plain value types carried in a
// std::variant. There is no RuleVisitor interface and no accept() method
// anywhere; std::visit dispatches on the variant's active alternative
// instead of a vtable walking through a pointer.
struct WeightAbove {
    double threshold_kg;
};

struct ZoneIs {
    DestinationZone zone;
};

struct DeclaredValueAbove {
    Cents threshold_cents;
};

struct RuleExpr;

// And/Or/Not hold their children behind unique_ptr<RuleExpr> because a
// std::variant cannot hold itself directly; the pointer is the only
// indirection a recursive variant needs, and it still owns its children
// outright, the same RAII ownership Chapter 8's RateEstimator used for a
// single strategy.
struct AndExpr {
    std::unique_ptr<RuleExpr> left;
    std::unique_ptr<RuleExpr> right;
};

struct OrExpr {
    std::unique_ptr<RuleExpr> left;
    std::unique_ptr<RuleExpr> right;
};

struct NotExpr {
    std::unique_ptr<RuleExpr> inner;
};

struct RuleExpr {
    std::variant<WeightAbove, ZoneIs, DeclaredValueAbove, AndExpr, OrExpr, NotExpr> node;
};

// A lambda-per-alternative overload set, deduced via CTAD, so a
// std::visit call reads as one operation with one branch per node kind
// instead of a single generic lambda full of std::holds_alternative
// checks.
template <typename... Alternatives>
struct Overload : Alternatives... {
    using Alternatives::operator()...;
};

template <typename... Alternatives>
Overload(Alternatives...) -> Overload<Alternatives...>;

RuleExpr weight_above(double threshold_kg);
RuleExpr zone_is(DestinationZone zone);
RuleExpr declared_value_above(Cents threshold_cents);
RuleExpr rule_and(RuleExpr left, RuleExpr right);
RuleExpr rule_or(RuleExpr left, RuleExpr right);
RuleExpr rule_not(RuleExpr inner);

// The Interpreter: walks the AST to evaluate the grammar it represents.
// This is the same tree shape the classic and variant Visitor operations
// traverse; only the question asked of each node differs, and the
// nesting of And/Or/Not nodes decides the answer the same way operator
// precedence would in a parsed expression.
bool evaluate(const RuleExpr& expr, const Shipment& shipment);

// A second operation over the same variant shape, added with no change
// to WeightAbove, ZoneIs, DeclaredValueAbove, AndExpr, OrExpr, or
// NotExpr: the Visitor force, expressed here without a RuleVisitor
// interface at all.
std::string describe(const RuleExpr& expr);

// Deep-clones a RuleExpr tree, the way Chapter 13's Prototype clones a
// concrete object. RuleSetEditor's Memento below uses this so a snapshot
// owns an independent tree instead of aliasing the live one a caller
// might still be mutating.
RuleExpr clone(const RuleExpr& expr);

}  // namespace rules
