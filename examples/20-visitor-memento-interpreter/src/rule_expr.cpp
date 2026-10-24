#include "rule_expr.hpp"

#include <utility>

namespace rules {

RuleExpr weight_above(double threshold_kg) {
    return RuleExpr{WeightAbove{threshold_kg}};
}

RuleExpr zone_is(DestinationZone zone) {
    return RuleExpr{ZoneIs{zone}};
}

RuleExpr declared_value_above(Cents threshold_cents) {
    return RuleExpr{DeclaredValueAbove{threshold_cents}};
}

RuleExpr rule_and(RuleExpr left, RuleExpr right) {
    return RuleExpr{AndExpr{std::make_unique<RuleExpr>(std::move(left)),
                             std::make_unique<RuleExpr>(std::move(right))}};
}

RuleExpr rule_or(RuleExpr left, RuleExpr right) {
    return RuleExpr{OrExpr{std::make_unique<RuleExpr>(std::move(left)),
                            std::make_unique<RuleExpr>(std::move(right))}};
}

RuleExpr rule_not(RuleExpr inner) {
    return RuleExpr{NotExpr{std::make_unique<RuleExpr>(std::move(inner))}};
}

bool evaluate(const RuleExpr& expr, const Shipment& shipment) {
    return std::visit(
        Overload{
            [&](const WeightAbove& predicate) { return shipment.weight_kg > predicate.threshold_kg; },
            [&](const ZoneIs& predicate) { return shipment.zone == predicate.zone; },
            [&](const DeclaredValueAbove& predicate) {
                return shipment.declared_value_cents > predicate.threshold_cents;
            },
            [&](const AndExpr& expr_and) {
                return evaluate(*expr_and.left, shipment) && evaluate(*expr_and.right, shipment);
            },
            [&](const OrExpr& expr_or) {
                return evaluate(*expr_or.left, shipment) || evaluate(*expr_or.right, shipment);
            },
            [&](const NotExpr& expr_not) { return !evaluate(*expr_not.inner, shipment); },
        },
        expr.node);
}

std::string describe(const RuleExpr& expr) {
    return std::visit(
        Overload{
            [](const WeightAbove& predicate) -> std::string {
                return "weight above " + shipping::format_weight_kg(predicate.threshold_kg);
            },
            [](const ZoneIs& predicate) -> std::string {
                return "zone is " + shipping::zone_name(predicate.zone);
            },
            [](const DeclaredValueAbove& predicate) -> std::string {
                return "declared value above " + shipping::format_cents(predicate.threshold_cents);
            },
            [](const AndExpr& expr_and) -> std::string {
                return "(" + describe(*expr_and.left) + " AND " + describe(*expr_and.right) + ")";
            },
            [](const OrExpr& expr_or) -> std::string {
                return "(" + describe(*expr_or.left) + " OR " + describe(*expr_or.right) + ")";
            },
            [](const NotExpr& expr_not) -> std::string { return "NOT " + describe(*expr_not.inner); },
        },
        expr.node);
}

RuleExpr clone(const RuleExpr& expr) {
    return std::visit(
        Overload{
            [](const WeightAbove& predicate) { return RuleExpr{predicate}; },
            [](const ZoneIs& predicate) { return RuleExpr{predicate}; },
            [](const DeclaredValueAbove& predicate) { return RuleExpr{predicate}; },
            [](const AndExpr& expr_and) {
                return RuleExpr{AndExpr{std::make_unique<RuleExpr>(clone(*expr_and.left)),
                                         std::make_unique<RuleExpr>(clone(*expr_and.right))}};
            },
            [](const OrExpr& expr_or) {
                return RuleExpr{OrExpr{std::make_unique<RuleExpr>(clone(*expr_or.left)),
                                        std::make_unique<RuleExpr>(clone(*expr_or.right))}};
            },
            [](const NotExpr& expr_not) {
                return RuleExpr{NotExpr{std::make_unique<RuleExpr>(clone(*expr_not.inner))}};
            },
        },
        expr.node);
}

}  // namespace rules
