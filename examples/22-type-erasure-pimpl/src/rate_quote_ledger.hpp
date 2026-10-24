#pragma once

#include <memory>
#include <string>

#include "shipping_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace shipping {

// RateQuoteLedger records every quote a checkout session asked for and can
// summarize them, but nothing about HOW it stores or formats that history
// is visible here. The only data member is a pointer to an incomplete
// type, so this header is a compilation firewall: changing how Impl
// stores entries, or adding a new statistic to the summary, never touches
// this file, and nothing that only includes rate_quote_ledger.hpp ever
// needs to recompile because of it. See Chapter 15's LabelRenderer for the
// PImpl mechanics this class reuses; this one goes further into why the
// firewall matters beyond saved recompiles.
class RateQuoteLedger {
public:
    RateQuoteLedger();
    ~RateQuoteLedger();

    RateQuoteLedger(const RateQuoteLedger& other);
    RateQuoteLedger& operator=(const RateQuoteLedger& other);
    RateQuoteLedger(RateQuoteLedger&& other) noexcept;
    RateQuoteLedger& operator=(RateQuoteLedger&& other) noexcept;

    // Throws std::invalid_argument if amount_cents is negative.
    void record(std::string label, Cents amount_cents);

    int entry_count() const;
    Cents total_cents() const;
    std::string summary() const;

#if __cplusplus >= 202302L
    // C++23 refinement: a typed rejection instead of a thrown exception.
    // Guarded so the mandatory C++20 build path never requires
    // std::expected.
    std::expected<void, std::string> try_record(std::string label, Cents amount_cents);
#endif

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace shipping
