#pragma once

#include <string>

#include "billing_domain.hpp"
#include "invoice_calculator.hpp"

namespace billing {

// SRP in effect: InvoiceFormatter's only job is turning an Invoice plus its
// already-computed Totals into text a human reads. It does not compute a
// total itself (InvoiceCalculator's job) and does not know whether the
// invoice it renders has ever been saved anywhere
// (InMemoryInvoiceRepository's job).
class InvoiceFormatter {
public:
    static std::string format(const Invoice& invoice, const Totals& totals);
};

}  // namespace billing
