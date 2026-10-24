// Billing an invoice, split along three SOLID principles: SRP keeps
// calculation, formatting, and persistence in three separate types; OCP
// lets a new kind of rule join the invoice total without editing
// InvoiceCalculator; LSP keeps a refined discount rule substitutable
// everywhere its parent is expected.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <string_view>

#include "invoice_calculator.hpp"
#include "invoice_formatter.hpp"
#include "invoice_repository.hpp"
#include "invoice_rules.hpp"

namespace {

using billing::Cents;
using billing::InvoiceRule;

void print_money(std::string_view label, Cents cents) {
    const bool negative = cents < 0;
    const Cents magnitude = negative ? -cents : cents;
    std::cout << label << ": " << (negative ? "-$" : "$") << magnitude / 100 << "."
              << (magnitude % 100 < 10 ? "0" : "") << magnitude % 100 << '\n';
}

// Takes any InvoiceRule by base-class reference: this function never needs
// to know, or care, whether it was handed a PercentageDiscountRule or a
// LoyaltyDiscountRule. That is Liskov substitution made concrete: both
// types honor the same contract, so both work here unchanged.
void print_rule_adjustment(const InvoiceRule& rule, Cents subtotal) {
    print_money(rule.name(), rule.adjustment_for(subtotal));
}

}  // namespace

int main() {
    using namespace billing;

    const Invoice invoice{
        "INV-1042",
        "Riverside Hardware Co.",
        {
            {"Website design", 1, 250000},
            {"Hosting (monthly)", 3, 1500},
        },
    };
    const Cents subtotal = invoice_subtotal(invoice);

    std::cout << "== Single Responsibility: calculate, format, persist ==\n";
    const PercentageTaxRule sales_tax(0.0825);
    const PercentageDiscountRule standard_discount(0.10);
    const FlatServiceFeeRule service_fee(2500);

    const InvoiceCalculator standard_calculator({sales_tax, standard_discount, service_fee});
    const Totals standard_totals = standard_calculator.totals_for(invoice);
    std::cout << InvoiceFormatter::format(invoice, standard_totals);

    InMemoryInvoiceRepository repository;
    repository.save(invoice);
    const auto reloaded = repository.find_by_id(invoice.id);
    std::cout << "Persisted and reloaded by id: " << (reloaded.has_value() && reloaded->id == invoice.id ? "yes" : "no")
              << "\n";

    std::cout << "\n== Open/Closed: three rule kinds, one calculator never edited ==\n";
    print_rule_adjustment(sales_tax, subtotal);
    print_rule_adjustment(standard_discount, subtotal);
    print_rule_adjustment(service_fee, subtotal);
    print_money("Invoice total", standard_totals.total);

    std::cout << "\n== Liskov Substitution: a loyalty discount stands in for the base rule ==\n";
    const LoyaltyDiscountRule loyalty_discount(/*base_rate=*/0.03, /*loyalty_years=*/4);
    print_rule_adjustment(standard_discount, subtotal);
    print_rule_adjustment(loyalty_discount, subtotal);

    const InvoiceCalculator loyalty_calculator({sales_tax, loyalty_discount, service_fee});
    const Totals loyalty_totals = loyalty_calculator.totals_for(invoice);
    print_money("Invoice total for a 4-year loyalty customer", loyalty_totals.total);

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed validation via std::expected ==\n";
    const Invoice empty_invoice{"INV-0000", "No Items Inc.", {}};
    if (const auto result = standard_calculator.try_totals_for(empty_invoice); !result.has_value()) {
        std::cout << "Rejected empty invoice: " << result.error() << '\n';
    }
#endif

    return 0;
}
