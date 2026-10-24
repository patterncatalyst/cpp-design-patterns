#pragma once

#include <gmock/gmock.h>

#include "quote_ledger.hpp"

namespace freight {

// A GoogleMock stand-in for the real ledger. QuoteIntake only ever calls
// record() through the QuoteLedger interface, so this mock plugs into the
// same constructor the demo binary uses with InMemoryQuoteLedger, but lets
// a test set expectations on exactly how QuoteIntake calls it.
class MockQuoteLedger : public QuoteLedger {
public:
    MOCK_METHOD(bool, record, (const Quote& quote), (override));
};

}  // namespace freight
