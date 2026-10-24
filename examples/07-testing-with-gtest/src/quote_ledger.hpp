#pragma once

#include <string>

#include "quote_domain.hpp"

namespace freight {

// A priced, identified shipment, ready to be handed to a ledger.
struct Quote {
    std::string quote_id;
    ShipmentRequest request;
    Cents amount_cents;
};

// The collaborator QuoteIntake depends on to make a quote durable.
// QuoteIntake never knows whether this writes to a database, a message
// queue, or (in this example's tests) a GoogleMock stand-in; it only knows
// the contract: record() returns true if the quote is now on file, false
// if the ledger declined it, for instance because quote_id already exists.
class QuoteLedger {
public:
    virtual ~QuoteLedger() = default;
    virtual bool record(const Quote& quote) = 0;
};

}  // namespace freight
