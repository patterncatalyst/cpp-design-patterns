#pragma once

#include <optional>

#include "quote_domain.hpp"
#include "quote_id_generator.hpp"
#include "quote_ledger.hpp"

namespace freight {

// The component this chapter's test suite exists to prove correct.
// QuoteIntake depends on two collaborators through their interfaces,
// QuoteLedger and QuoteIdGenerator, both injected by reference and owned
// by the caller, never by QuoteIntake itself. That is what lets
// tests/quote_intake_test.cpp swap a GoogleMock QuoteLedger in for the
// real one without QuoteIntake's own code knowing the difference.
class QuoteIntake {
public:
    QuoteIntake(QuoteLedger& ledger, QuoteIdGenerator& id_generator);

    // Validates the request, prices it, and asks the ledger to record it.
    // Returns the recorded Quote on success; returns std::nullopt if the
    // request fails validation (the ledger is never called) or if the
    // ledger itself declines to record it.
    std::optional<Quote> submit(const ShipmentRequest& request);

private:
    QuoteLedger& ledger_;
    QuoteIdGenerator& id_generator_;
};

}  // namespace freight
