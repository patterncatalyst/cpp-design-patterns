#include "approval_chain.hpp"

#include <algorithm>
#include <sstream>

namespace shipping {

WeightLimitHandler::WeightLimitHandler(double max_kg, std::unique_ptr<ApprovalHandler> next)
    : ApprovalHandler("WeightLimitHandler", std::move(next)), max_kg_(max_kg) {}

std::optional<ApprovalDecision> WeightLimitHandler::try_handle(const ShipmentRequest& request) const {
    if (request.weight_kg > max_kg_) {
        std::ostringstream reason;
        reason << "shipment weight " << request.weight_kg << " kg exceeds the " << max_kg_ << " kg limit";
        return ApprovalDecision{ApprovalOutcome::Rejected, name(), reason.str()};
    }
    return std::nullopt;
}

HazmatHandler::HazmatHandler(std::vector<std::string> cleared_countries, std::unique_ptr<ApprovalHandler> next)
    : ApprovalHandler("HazmatHandler", std::move(next)), cleared_countries_(std::move(cleared_countries)) {}

std::optional<ApprovalDecision> HazmatHandler::try_handle(const ShipmentRequest& request) const {
    if (!request.hazmat) {
        return std::nullopt;
    }
    const bool cleared = std::find(cleared_countries_.begin(), cleared_countries_.end(),
                                    request.destination_country) != cleared_countries_.end();
    if (!cleared) {
        return ApprovalDecision{ApprovalOutcome::Rejected, name(),
                                 "hazmat shipments are not cleared for " + request.destination_country};
    }
    return std::nullopt;
}

EmbargoHandler::EmbargoHandler(std::vector<std::string> embargoed_countries, std::unique_ptr<ApprovalHandler> next)
    : ApprovalHandler("EmbargoHandler", std::move(next)), embargoed_countries_(std::move(embargoed_countries)) {}

std::optional<ApprovalDecision> EmbargoHandler::try_handle(const ShipmentRequest& request) const {
    const bool embargoed = std::find(embargoed_countries_.begin(), embargoed_countries_.end(),
                                      request.destination_country) != embargoed_countries_.end();
    if (embargoed) {
        return ApprovalDecision{ApprovalOutcome::Rejected, name(),
                                 "destination " + request.destination_country + " is under embargo"};
    }
    return std::nullopt;
}

ManualApprovalHandler::ManualApprovalHandler() : ApprovalHandler("ManualApprovalHandler", nullptr) {}

std::optional<ApprovalDecision> ManualApprovalHandler::try_handle(const ShipmentRequest& /*request*/) const {
    return ApprovalDecision{ApprovalOutcome::NeedsManualReview, name(), "routed to the manual review queue"};
}

std::unique_ptr<ApprovalHandler> build_default_chain() {
    auto fallback = std::make_unique<ManualApprovalHandler>();
    auto embargo = std::make_unique<EmbargoHandler>(std::vector<std::string>{"KP", "IR"}, std::move(fallback));
    auto hazmat = std::make_unique<HazmatHandler>(std::vector<std::string>{"US", "CA"}, std::move(embargo));
    return std::make_unique<WeightLimitHandler>(40.0, std::move(hazmat));
}

}  // namespace shipping
