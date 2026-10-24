#include "quote_intake.hpp"

namespace freight {

QuoteIntake::QuoteIntake(QuoteLedger& ledger, QuoteIdGenerator& id_generator)
    : ledger_(ledger), id_generator_(id_generator) {}

std::optional<Quote> QuoteIntake::submit(const ShipmentRequest& request) {
    if (validate_shipment(request) != ValidationOutcome::Accepted) {
        return std::nullopt;
    }

    Quote quote{id_generator_.next_id(), request, quote_cost(request)};
    if (!ledger_.record(quote)) {
        return std::nullopt;
    }
    return quote;
}

}  // namespace freight
