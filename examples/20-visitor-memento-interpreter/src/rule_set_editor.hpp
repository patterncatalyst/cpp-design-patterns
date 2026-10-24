#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "rule_expr.hpp"
#include "shipping_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace rules {

// A single named filter rule inside a draft rule set. RuleExpr owns its
// own AST through the unique_ptr children on And/Or/Not, so a NamedRule
// is move-only, the same as RuleExpr itself.
struct NamedRule {
    std::string label;
    RuleExpr expr;
};

// Originator: a merchant's in-progress shipping-filter rule set. An admin
// screen edits one of these across several steps, renaming the draft,
// adding and removing rules, before publishing it. Memento below lets
// that screen snapshot the whole thing in one call and roll an entire
// batch of edits back in one call, instead of reversing each edit in
// turn the way Chapter 11's CommandHistory undoes one InventoryCommand
// at a time.
class RuleSetEditor {
public:
    // Memento: an opaque snapshot of RuleSetEditor's entire state at one
    // moment. Only RuleSetEditor itself can read what is inside one
    // (Snapshot's constructor and members are private, with RuleSetEditor
    // alone named as a friend); a caretaker can hold a Snapshot, move it
    // around, and hand it back to restore(), but cannot inspect or alter
    // the rules frozen inside it.
    class Snapshot {
    public:
        Snapshot(Snapshot&&) = default;
        Snapshot& operator=(Snapshot&&) = default;
        Snapshot(const Snapshot&) = delete;
        Snapshot& operator=(const Snapshot&) = delete;

    private:
        friend class RuleSetEditor;
        Snapshot(std::string draft_name, std::vector<NamedRule> rules, bool published);

        std::string draft_name_;
        std::vector<NamedRule> rules_;
        bool published_;
    };

    void set_draft_name(std::string name);
    void add_rule(std::string label, RuleExpr expr);
    bool remove_rule(const std::string& label);
    void set_published(bool published);

    // Capture the whole current state as an opaque Snapshot. Every rule's
    // RuleExpr is deep-cloned, so later mutation of this editor cannot
    // reach into a Snapshot already handed to a caretaker.
    Snapshot save() const;

    // Replace this editor's entire state with what a Snapshot captured,
    // wholesale, not one field or one rule at a time.
    void restore(Snapshot snapshot);

    const std::string& draft_name() const { return draft_name_; }
    std::size_t rule_count() const { return rules_.size(); }
    bool is_published() const { return published_; }
    bool has_rule(const std::string& label) const;

    // Always available: throws if the label is missing, the plain C++20
    // way to report a lookup failure.
    bool evaluate_rule(const std::string& label, const Shipment& shipment) const;

#if __cplusplus >= 202302L
    // C++23 refinement: the same lookup, reported as a typed error
    // instead of an exception. Guarded so the mandatory C++20 build path
    // never needs std::expected.
    std::expected<bool, std::string> try_evaluate_rule(const std::string& label,
                                                         const Shipment& shipment) const;
#endif

private:
    const NamedRule* find_rule(const std::string& label) const;

    std::string draft_name_;
    std::vector<NamedRule> rules_;
    bool published_ = false;
};

}  // namespace rules
