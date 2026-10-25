#include "strangler_pricing_router.hpp"

#include <utility>

namespace strangler30 {

void CutoverPolicy::migrate_customer(const std::string& customer_id) {
    migrated_customers_.insert(customer_id);
}

bool CutoverPolicy::is_migrated(const std::string& customer_id) const {
    return migrated_customers_.find(customer_id) != migrated_customers_.end();
}

EngineChoice CutoverPolicy::choose(const ShipmentRequest& request) const {
    return is_migrated(request.customer_id) ? EngineChoice::Modern : EngineChoice::Legacy;
}

std::size_t CutoverPolicy::migrated_count() const {
    return migrated_customers_.size();
}

StranglerPricingRouter::StranglerPricingRouter(std::shared_ptr<PricingEngine> legacy,
                                                std::shared_ptr<PricingEngine> modern,
                                                CutoverPolicy policy)
    : legacy_(std::move(legacy)), modern_(std::move(modern)), policy_(std::move(policy)) {}

Quote StranglerPricingRouter::quote(const ShipmentRequest& request) const {
    switch (mode_) {
        case Mode::LegacyOnly:
            return legacy_->quote(request);
        case Mode::ModernOnly:
            return modern_->quote(request);
        case Mode::Compare: {
            const Quote legacy_quote = legacy_->quote(request);
            const Quote modern_quote = modern_->quote(request);
            if (legacy_quote.total_cents != modern_quote.total_cents) {
                disagreements_.push_back(QuoteDisagreement{request, legacy_quote, modern_quote});
            }
            // Legacy stays authoritative for every customer-facing answer
            // until the comparison window closes with zero disagreements;
            // Compare mode exists to find bugs, not to ship them.
            return legacy_quote;
        }
        case Mode::Routed:
            return policy_.choose(request) == EngineChoice::Modern ? modern_->quote(request)
                                                                     : legacy_->quote(request);
    }
    return legacy_->quote(request);  // unreachable; keeps -Wreturn-type quiet
}

void StranglerPricingRouter::set_mode(Mode mode) {
    mode_ = mode;
}

StranglerPricingRouter::Mode StranglerPricingRouter::mode() const {
    return mode_;
}

CutoverPolicy& StranglerPricingRouter::policy() {
    return policy_;
}

const std::vector<QuoteDisagreement>& StranglerPricingRouter::disagreements() const {
    return disagreements_;
}

}  // namespace strangler30
