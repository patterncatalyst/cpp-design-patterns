#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace shipping {

// Chain of Responsibility: a shipment request passes down a chain of
// handlers until one of them acts on it. Approved never actually appears
// below; every handler in this example either rejects a request outright
// or passes it on, and the fallback at the end of the chain always routes
// a surviving request to a human reviewer rather than auto-approving it.
enum class ApprovalOutcome {
    Approved,
    Rejected,
    NeedsManualReview,
};

struct ShipmentRequest {
    double weight_kg;
    bool hazmat;
    std::string destination_country;
};

struct ApprovalDecision {
    ApprovalOutcome outcome;
    std::string handler_name;
    std::string reason;
};

// Base handler. handle() logs which handler it consulted, then either
// returns its own decision (try_handle found one) or delegates to the next
// handler in the chain. A caller holding only the head of the chain cannot
// tell how many handlers actually looked at a request before one of them
// decided it; call_log exists so this example's tests can observe that
// directly instead of inferring it from the final decision alone.
class ApprovalHandler {
public:
    explicit ApprovalHandler(std::string name, std::unique_ptr<ApprovalHandler> next = nullptr)
        : name_(std::move(name)), next_(std::move(next)) {}
    virtual ~ApprovalHandler() = default;

    ApprovalDecision handle(const ShipmentRequest& request, std::vector<std::string>& call_log) const {
        call_log.push_back(name_);
        if (auto decision = try_handle(request)) {
            return *decision;
        }
        if (next_) {
            return next_->handle(request, call_log);
        }
        return ApprovalDecision{ApprovalOutcome::NeedsManualReview, name_,
                                 "no handler in the chain reached a decision"};
    }

    const std::string& name() const { return name_; }

protected:
    // Returns a decision if this handler acts on the request, or
    // std::nullopt to pass it to the next handler in the chain.
    virtual std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const = 0;

private:
    std::string name_;
    std::unique_ptr<ApprovalHandler> next_;
};

class WeightLimitHandler : public ApprovalHandler {
public:
    WeightLimitHandler(double max_kg, std::unique_ptr<ApprovalHandler> next);

protected:
    std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const override;

private:
    double max_kg_;
};

class HazmatHandler : public ApprovalHandler {
public:
    HazmatHandler(std::vector<std::string> cleared_countries, std::unique_ptr<ApprovalHandler> next);

protected:
    std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const override;

private:
    std::vector<std::string> cleared_countries_;
};

class EmbargoHandler : public ApprovalHandler {
public:
    EmbargoHandler(std::vector<std::string> embargoed_countries, std::unique_ptr<ApprovalHandler> next);

protected:
    std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const override;

private:
    std::vector<std::string> embargoed_countries_;
};

// Fallback handler. It has no next handler and its try_handle() always
// returns a decision, so a chain built with build_default_chain() below
// always terminates: a request that clears every automatic check still
// lands somewhere, a manual-review queue, rather than falling off the end
// of the chain with no decision at all.
class ManualApprovalHandler : public ApprovalHandler {
public:
    ManualApprovalHandler();

protected:
    std::optional<ApprovalDecision> try_handle(const ShipmentRequest& request) const override;
};

// Builds the chain this example exercises: WeightLimitHandler ->
// HazmatHandler -> EmbargoHandler -> ManualApprovalHandler (fallback).
std::unique_ptr<ApprovalHandler> build_default_chain();

}  // namespace shipping
