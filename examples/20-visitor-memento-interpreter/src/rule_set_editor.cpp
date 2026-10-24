#include "rule_set_editor.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rules {

RuleSetEditor::Snapshot::Snapshot(std::string draft_name, std::vector<NamedRule> rules, bool published)
    : draft_name_(std::move(draft_name)), rules_(std::move(rules)), published_(published) {}

void RuleSetEditor::set_draft_name(std::string name) {
    draft_name_ = std::move(name);
}

void RuleSetEditor::add_rule(std::string label, RuleExpr expr) {
    rules_.push_back(NamedRule{std::move(label), std::move(expr)});
}

bool RuleSetEditor::remove_rule(const std::string& label) {
    const auto it = std::find_if(rules_.begin(), rules_.end(),
                                  [&](const NamedRule& rule) { return rule.label == label; });
    if (it == rules_.end()) {
        return false;
    }
    rules_.erase(it);
    return true;
}

void RuleSetEditor::set_published(bool published) {
    published_ = published;
}

const NamedRule* RuleSetEditor::find_rule(const std::string& label) const {
    const auto it = std::find_if(rules_.begin(), rules_.end(),
                                  [&](const NamedRule& rule) { return rule.label == label; });
    return it == rules_.end() ? nullptr : &(*it);
}

bool RuleSetEditor::has_rule(const std::string& label) const {
    return find_rule(label) != nullptr;
}

bool RuleSetEditor::evaluate_rule(const std::string& label, const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        throw std::out_of_range("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}

#if __cplusplus >= 202302L
std::expected<bool, std::string> RuleSetEditor::try_evaluate_rule(const std::string& label,
                                                                    const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        return std::unexpected<std::string>("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}
#endif

RuleSetEditor::Snapshot RuleSetEditor::save() const {
    std::vector<NamedRule> cloned_rules;
    cloned_rules.reserve(rules_.size());
    for (const NamedRule& rule : rules_) {
        cloned_rules.push_back(NamedRule{rule.label, clone(rule.expr)});
    }
    return Snapshot(draft_name_, std::move(cloned_rules), published_);
}

void RuleSetEditor::restore(Snapshot snapshot) {
    draft_name_ = std::move(snapshot.draft_name_);
    rules_ = std::move(snapshot.rules_);
    published_ = snapshot.published_;
}

}  // namespace rules
