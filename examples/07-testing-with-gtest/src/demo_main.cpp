#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "quote_domain.hpp"
#include "quote_id_generator.hpp"
#include "quote_intake.hpp"
#include "quote_ledger.hpp"

namespace {

using freight::Cents;
using freight::Quote;
using freight::QuoteLedger;
using freight::ServiceZone;
using freight::ShipmentRequest;

// A minimal in-memory ledger for this demo binary. The test suite never
// uses this class; it uses a GoogleMock MockQuoteLedger instead, so it can
// assert on *how* QuoteIntake calls record(), not only on what a fake
// implementation happens to do with the result.
class InMemoryQuoteLedger : public QuoteLedger {
public:
    bool record(const Quote& quote) override {
        for (const auto& existing : quotes_) {
            if (existing.quote_id == quote.quote_id) {
                return false;
            }
        }
        quotes_.push_back(quote);
        return true;
    }

    const std::vector<Quote>& quotes() const { return quotes_; }

private:
    std::vector<Quote> quotes_;
};

std::string format_cents(Cents cents) {
    std::ostringstream out;
    out << '$' << std::fixed << std::setprecision(2) << (static_cast<double>(cents) / 100.0);
    return out.str();
}

}  // namespace

int main() {
    using freight::describe;
    using freight::validate_shipment;

    std::cout << "== Validation: checking five requests before any of them is priced ==\n";
    const std::vector<ShipmentRequest> samples{
        {3.5, ServiceZone::Regional, 12'000},
        {0.0, ServiceZone::Local, 500},
        {-2.0, ServiceZone::National, 500},
        {1.0, ServiceZone::Unassigned, 500},
        {1.0, ServiceZone::International, 10'000'000},
    };
    for (const auto& request : samples) {
        std::cout << "  weight=" << request.weight_kg << "kg -> " << describe(validate_shipment(request)) << "\n";
    }

    std::cout << "\n== Intake: validating, pricing, and recording through the ledger ==\n";
    freight::SequentialQuoteIdGenerator id_generator;
    InMemoryQuoteLedger ledger;
    freight::QuoteIntake intake(ledger, id_generator);

    const ShipmentRequest regional{3.5, ServiceZone::Regional, 12'000};
    if (const auto quote = intake.submit(regional)) {
        std::cout << "  " << quote->quote_id << ": " << format_cents(quote->amount_cents) << " (Regional, 3.5kg)\n";
    }

    const ShipmentRequest international{8.0, ServiceZone::International, 40'000};
    if (const auto quote = intake.submit(international)) {
        std::cout << "  " << quote->quote_id << ": " << format_cents(quote->amount_cents)
                   << " (International, 8kg)\n";
    }

    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 100};
    if (!intake.submit(malformed)) {
        std::cout << "  Rejected before pricing: " << describe(validate_shipment(malformed)) << "\n";
    }

    std::cout << "\n== Ledger decline: the ledger, not validation, says no ==\n";
    // Seed the ledger with the id the generator is about to hand out next,
    // the same way a retried write could collide with a real database
    // sequence. QuoteIntake still prices the shipment, but submit() returns
    // std::nullopt because record() reports the id already on file.
    ledger.record(Quote{"Q-0003", regional, 0});
    const ShipmentRequest third{2.0, ServiceZone::Local, 100};
    if (!intake.submit(third)) {
        std::cout << "  Ledger declined to record the quote: quote_id already on file\n";
    }

    std::cout << "\nLedger now holds " << ledger.quotes().size() << " quotes.\n";
    return 0;
}
