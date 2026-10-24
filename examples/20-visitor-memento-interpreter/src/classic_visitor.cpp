#include "classic_visitor.hpp"

#include <utility>

namespace rules {

WeightAboveNode::WeightAboveNode(double threshold_kg) : threshold_kg_(threshold_kg) {}
void WeightAboveNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

ZoneIsNode::ZoneIsNode(DestinationZone zone) : zone_(zone) {}
void ZoneIsNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

DeclaredValueAboveNode::DeclaredValueAboveNode(Cents threshold_cents) : threshold_cents_(threshold_cents) {}
void DeclaredValueAboveNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

AndNode::AndNode(std::unique_ptr<RuleNode> left, std::unique_ptr<RuleNode> right)
    : left_(std::move(left)), right_(std::move(right)) {}
void AndNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

OrNode::OrNode(std::unique_ptr<RuleNode> left, std::unique_ptr<RuleNode> right)
    : left_(std::move(left)), right_(std::move(right)) {}
void OrNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

NotNode::NotNode(std::unique_ptr<RuleNode> inner) : inner_(std::move(inner)) {}
void NotNode::accept(RuleVisitor& visitor) const { visitor.visit(*this); }

MatchEvaluator::MatchEvaluator(const Shipment& shipment) : shipment_(shipment) {}

void MatchEvaluator::visit(const WeightAboveNode& node) {
    result_ = shipment_.weight_kg > node.threshold_kg();
}

void MatchEvaluator::visit(const ZoneIsNode& node) {
    result_ = shipment_.zone == node.zone();
}

void MatchEvaluator::visit(const DeclaredValueAboveNode& node) {
    result_ = shipment_.declared_value_cents > node.threshold_cents();
}

void MatchEvaluator::visit(const AndNode& node) {
    MatchEvaluator left_eval(shipment_);
    node.left().accept(left_eval);
    MatchEvaluator right_eval(shipment_);
    node.right().accept(right_eval);
    result_ = left_eval.result() && right_eval.result();
}

void MatchEvaluator::visit(const OrNode& node) {
    MatchEvaluator left_eval(shipment_);
    node.left().accept(left_eval);
    MatchEvaluator right_eval(shipment_);
    node.right().accept(right_eval);
    result_ = left_eval.result() || right_eval.result();
}

void MatchEvaluator::visit(const NotNode& node) {
    MatchEvaluator inner_eval(shipment_);
    node.inner().accept(inner_eval);
    result_ = !inner_eval.result();
}

void RuleDescriber::visit(const WeightAboveNode& node) {
    out_ = "weight above " + shipping::format_weight_kg(node.threshold_kg());
}

void RuleDescriber::visit(const ZoneIsNode& node) {
    out_ = "zone is " + zone_name(node.zone());
}

void RuleDescriber::visit(const DeclaredValueAboveNode& node) {
    out_ = "declared value above " + shipping::format_cents(node.threshold_cents());
}

void RuleDescriber::visit(const AndNode& node) {
    RuleDescriber left_desc;
    node.left().accept(left_desc);
    RuleDescriber right_desc;
    node.right().accept(right_desc);
    out_ = "(" + left_desc.result() + " AND " + right_desc.result() + ")";
}

void RuleDescriber::visit(const OrNode& node) {
    RuleDescriber left_desc;
    node.left().accept(left_desc);
    RuleDescriber right_desc;
    node.right().accept(right_desc);
    out_ = "(" + left_desc.result() + " OR " + right_desc.result() + ")";
}

void RuleDescriber::visit(const NotNode& node) {
    RuleDescriber inner_desc;
    node.inner().accept(inner_desc);
    out_ = "NOT " + inner_desc.result();
}

bool evaluate_classic(const RuleNode& root, const Shipment& shipment) {
    MatchEvaluator evaluator(shipment);
    root.accept(evaluator);
    return evaluator.result();
}

std::string describe_classic(const RuleNode& root) {
    RuleDescriber describer;
    root.accept(describer);
    return describer.result();
}

}  // namespace rules
