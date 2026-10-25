#pragma once

#include "pricing_engine.hpp"

#include <memory>
#include <set>
#include <vector>

namespace strangler30 {

enum class EngineChoice { Legacy, Modern };

// The gradual-cutover policy: which customers' traffic has already moved
// to ModernPricingEngine. Starts empty, every customer on legacy, and
// grows one customer at a time, one release at a time; nothing about this
// type requires the whole migration to happen in a single step.
class CutoverPolicy {
public:
    void migrate_customer(const std::string& customer_id);
    bool is_migrated(const std::string& customer_id) const;
    EngineChoice choose(const ShipmentRequest& request) const;
    std::size_t migrated_count() const;

private:
    std::set<std::string> migrated_customers_;
};

struct QuoteDisagreement {
    ShipmentRequest request;
    Quote legacy_quote;
    Quote modern_quote;
};

// The strangler fig seam itself. Callers depend on PricingEngine exactly
// as they would if LegacyPricingEngine were still the only implementation;
// this type is what lets the system run the old engine, the new one, or
// both at once, with the choice changeable at runtime instead of at a
// recompile.
class StranglerPricingRouter : public PricingEngine {
public:
    enum class Mode {
        LegacyOnly,  // rollback target / pre-migration default
        ModernOnly,  // post-cutover; legacy no longer consulted
        Compare,     // run both, log disagreements, trust legacy's answer
        Routed,      // CutoverPolicy decides per request
    };

    StranglerPricingRouter(std::shared_ptr<PricingEngine> legacy,
                            std::shared_ptr<PricingEngine> modern,
                            CutoverPolicy policy);

    Quote quote(const ShipmentRequest& request) const override;

    void set_mode(Mode mode);
    Mode mode() const;
    CutoverPolicy& policy();
    const std::vector<QuoteDisagreement>& disagreements() const;

private:
    std::shared_ptr<PricingEngine> legacy_;
    std::shared_ptr<PricingEngine> modern_;
    CutoverPolicy policy_;
    Mode mode_ = Mode::LegacyOnly;
    mutable std::vector<QuoteDisagreement> disagreements_;
};

}  // namespace strangler30
