#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "billing_domain.hpp"

namespace billing {

// SRP in effect: InMemoryInvoiceRepository's only job is remembering
// invoices by id and handing them back. It never computes a total
// (InvoiceCalculator's job) and never renders one as text
// (InvoiceFormatter's job); it stores and retrieves the Invoice value
// exactly as it was given.
class InMemoryInvoiceRepository {
public:
    void save(Invoice invoice);
    std::optional<Invoice> find_by_id(const std::string& id) const;

private:
    std::unordered_map<std::string, Invoice> invoices_by_id_;
};

}  // namespace billing
