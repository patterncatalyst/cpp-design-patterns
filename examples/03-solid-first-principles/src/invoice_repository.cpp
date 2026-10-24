#include "invoice_repository.hpp"

#include <utility>

namespace billing {

void InMemoryInvoiceRepository::save(Invoice invoice) {
    const std::string id = invoice.id;
    invoices_by_id_[id] = std::move(invoice);
}

std::optional<Invoice> InMemoryInvoiceRepository::find_by_id(const std::string& id) const {
    const auto it = invoices_by_id_.find(id);
    if (it == invoices_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace billing
