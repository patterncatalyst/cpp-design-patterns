#pragma once

#include <memory>
#include <string>

#include "shipping_domain.hpp"

namespace rules {

using shipping::Cents;
using shipping::DestinationZone;
using shipping::Shipment;
using shipping::zone_name;

class RuleVisitor;

// Visitor's fixed object structure: a small rule-node AST a merchant's
// shipping filter is built from. New node KINDS are rare, the grammar is
// fixed; new OPERATIONS over the tree (does this shipment match, render a
// readable string, sum a surcharge, ...) are common. That asymmetry is
// exactly the force Visitor answers: add an operation without touching a
// single RuleNode subclass.
class RuleNode {
public:
    virtual ~RuleNode() = default;
    virtual void accept(RuleVisitor& visitor) const = 0;
};

class WeightAboveNode : public RuleNode {
public:
    explicit WeightAboveNode(double threshold_kg);
    void accept(RuleVisitor& visitor) const override;
    double threshold_kg() const { return threshold_kg_; }

private:
    double threshold_kg_;
};

class ZoneIsNode : public RuleNode {
public:
    explicit ZoneIsNode(DestinationZone zone);
    void accept(RuleVisitor& visitor) const override;
    DestinationZone zone() const { return zone_; }

private:
    DestinationZone zone_;
};

class DeclaredValueAboveNode : public RuleNode {
public:
    explicit DeclaredValueAboveNode(Cents threshold_cents);
    void accept(RuleVisitor& visitor) const override;
    Cents threshold_cents() const { return threshold_cents_; }

private:
    Cents threshold_cents_;
};

class AndNode : public RuleNode {
public:
    AndNode(std::unique_ptr<RuleNode> left, std::unique_ptr<RuleNode> right);
    void accept(RuleVisitor& visitor) const override;
    const RuleNode& left() const { return *left_; }
    const RuleNode& right() const { return *right_; }

private:
    std::unique_ptr<RuleNode> left_;
    std::unique_ptr<RuleNode> right_;
};

class OrNode : public RuleNode {
public:
    OrNode(std::unique_ptr<RuleNode> left, std::unique_ptr<RuleNode> right);
    void accept(RuleVisitor& visitor) const override;
    const RuleNode& left() const { return *left_; }
    const RuleNode& right() const { return *right_; }

private:
    std::unique_ptr<RuleNode> left_;
    std::unique_ptr<RuleNode> right_;
};

class NotNode : public RuleNode {
public:
    explicit NotNode(std::unique_ptr<RuleNode> inner);
    void accept(RuleVisitor& visitor) const override;
    const RuleNode& inner() const { return *inner_; }

private:
    std::unique_ptr<RuleNode> inner_;
};

// One pure virtual method per concrete node kind: the GoF double-dispatch
// shape. accept() calls visitor.visit(*this), resolving the node's exact
// type at the first dispatch; visit() is already overloaded on that exact
// type, resolving the operation at the second. A new RuleVisitor
// implementation below needs no change to RuleNode or any of its
// subclasses.
class RuleVisitor {
public:
    virtual ~RuleVisitor() = default;
    virtual void visit(const WeightAboveNode& node) = 0;
    virtual void visit(const ZoneIsNode& node) = 0;
    virtual void visit(const DeclaredValueAboveNode& node) = 0;
    virtual void visit(const AndNode& node) = 0;
    virtual void visit(const OrNode& node) = 0;
    virtual void visit(const NotNode& node) = 0;
};

// First operation: does this shipment match the rule? Each visit() that
// recurses (And/Or/Not) builds a fresh MatchEvaluator per child, because
// visit() has no return value to hand a result back through; stashing the
// child's answer in a throwaway evaluator and reading it back is the
// idiomatic way a GoF visitor computes a recursive result.
class MatchEvaluator : public RuleVisitor {
public:
    explicit MatchEvaluator(const Shipment& shipment);
    bool result() const { return result_; }

    void visit(const WeightAboveNode& node) override;
    void visit(const ZoneIsNode& node) override;
    void visit(const DeclaredValueAboveNode& node) override;
    void visit(const AndNode& node) override;
    void visit(const OrNode& node) override;
    void visit(const NotNode& node) override;

private:
    const Shipment& shipment_;
    bool result_ = false;
};

// Second operation, added with zero changes to RuleNode or any of its six
// subclasses: render the rule as a readable string for an audit log.
class RuleDescriber : public RuleVisitor {
public:
    std::string result() const { return out_; }

    void visit(const WeightAboveNode& node) override;
    void visit(const ZoneIsNode& node) override;
    void visit(const DeclaredValueAboveNode& node) override;
    void visit(const AndNode& node) override;
    void visit(const OrNode& node) override;
    void visit(const NotNode& node) override;

private:
    std::string out_;
};

bool evaluate_classic(const RuleNode& root, const Shipment& shipment);
std::string describe_classic(const RuleNode& root);

}  // namespace rules
