#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "classic_visitor.hpp"
#include "rule_expr.hpp"
#include "rule_set_editor.hpp"
#include "shipping_domain.hpp"

using shipping::DestinationZone;
using shipping::Shipment;

namespace {

void print_classic_vs_variant() {
    std::cout << "== Visitor 1: classic double dispatch vs Visitor 2: std::variant + std::visit ==\n";

    const Shipment shipment{12.0, DestinationZone::National, 30000};

    // Classic: a class hierarchy, a RuleVisitor interface, accept()/visit().
    rules::AndNode classic_tree(std::make_unique<rules::WeightAboveNode>(10.0),
                                 std::make_unique<rules::ZoneIsNode>(DestinationZone::National));

    const bool classic_match = rules::evaluate_classic(classic_tree, shipment);
    const std::string classic_description = rules::describe_classic(classic_tree);

    // Variant: the same two predicates, combined the same way, as value
    // types in a RuleExpr. No RuleVisitor interface and no accept()
    // anywhere.
    rules::RuleExpr variant_tree =
        rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National));

    const bool variant_match = rules::evaluate(variant_tree, shipment);
    const std::string variant_description = rules::describe(variant_tree);

    std::cout << "Classic visitor:  match=" << std::boolalpha << classic_match << "  " << classic_description
              << "\n";
    std::cout << "Variant visitor:  match=" << std::boolalpha << variant_match << "  " << variant_description
              << "\n";
    std::cout << (classic_match == variant_match && classic_description == variant_description
                      ? "Both representations agree on match and description.\n"
                      : "MISMATCH\n");
}

void print_interpreter_precedence() {
    std::cout << "\n== Interpreter: nesting changes the evaluated result ==\n";

    const Shipment shipment{5.0, DestinationZone::National, 80000};

    rules::RuleExpr expr_outer_or =
        rules::rule_or(rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National)),
                        rules::declared_value_above(50000));

    rules::RuleExpr expr_outer_and = rules::rule_and(
        rules::weight_above(10.0),
        rules::rule_or(rules::zone_is(DestinationZone::National), rules::declared_value_above(50000)));

    std::cout << "(weight above 10.0kg AND zone is National) OR declared value above $500.00\n";
    std::cout << "  -> " << std::boolalpha << rules::evaluate(expr_outer_or, shipment) << "\n";
    std::cout << "weight above 10.0kg AND (zone is National OR declared value above $500.00)\n";
    std::cout << "  -> " << std::boolalpha << rules::evaluate(expr_outer_and, shipment) << "\n";
    std::cout << "Same three predicates, different nesting, different answer for the identical shipment.\n";
}

void print_memento() {
    std::cout << "\n== Memento: snapshotting and restoring a whole draft rule set ==\n";

    const Shipment shipment{5.0, DestinationZone::National, 80000};

    rules::RuleSetEditor editor;
    editor.set_draft_name("Q4-international-filters");
    editor.add_rule("high-value-national",
                     rules::rule_or(rules::rule_and(rules::weight_above(10.0),
                                                     rules::zone_is(DestinationZone::National)),
                                    rules::declared_value_above(50000)));
    editor.set_published(false);

    std::cout << "Before edits:  name=" << editor.draft_name() << " rules=" << editor.rule_count()
              << " published=" << std::boolalpha << editor.is_published() << "\n";

    rules::RuleSetEditor::Snapshot snapshot = editor.save();

    editor.set_draft_name("DRAFT-EDIT");
    editor.add_rule("temp-rule", rules::weight_above(999.0));
    editor.set_published(true);

    std::cout << "After edits:   name=" << editor.draft_name() << " rules=" << editor.rule_count()
              << " published=" << std::boolalpha << editor.is_published() << "\n";

    editor.restore(std::move(snapshot));

    std::cout << "After restore: name=" << editor.draft_name() << " rules=" << editor.rule_count()
              << " published=" << std::boolalpha << editor.is_published() << "\n";
    std::cout << "Restored rule evaluates: " << std::boolalpha
              << editor.evaluate_rule("high-value-national", shipment) << "\n";

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed lookup error via std::expected ==\n";
    const auto missing = editor.try_evaluate_rule("ghost-rule", shipment);
    if (!missing.has_value()) {
        std::cout << "Rejected lookup: " << missing.error() << "\n";
    }
#endif
}

}  // namespace

int main() {
    print_classic_vs_variant();
    print_interpreter_precedence();
    print_memento();
    return 0;
}
