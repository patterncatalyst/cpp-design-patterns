#include "invoice_formatter.hpp"

#include <gtest/gtest.h>

namespace billing {
namespace {

TEST(InvoiceFormatterTest, RendersTheIdCustomerLineItemAndTotals) {
    const Invoice invoice{
        "INV-7",
        "Example Customer",
        {
            {"Consulting", 2, 10000},
        },
    };
    const Totals totals{20000, 20000};

    const std::string rendered = InvoiceFormatter::format(invoice, totals);

    EXPECT_NE(rendered.find("INV-7"), std::string::npos);
    EXPECT_NE(rendered.find("Example Customer"), std::string::npos);
    EXPECT_NE(rendered.find("2 x Consulting"), std::string::npos);
    EXPECT_NE(rendered.find("Subtotal: $200.00"), std::string::npos);
    EXPECT_NE(rendered.find("Total:    $200.00"), std::string::npos);
}

TEST(InvoiceFormatterTest, FormatsALineItemExtendedAmountNotJustTheUnitPrice) {
    const Invoice invoice{
        "INV-8",
        "Example Customer",
        {
            {"Hosting", 3, 1500},
        },
    };
    const Totals totals{4500, 4500};

    const std::string rendered = InvoiceFormatter::format(invoice, totals);

    EXPECT_NE(rendered.find("@ $15.00 = $45.00"), std::string::npos);
}

TEST(InvoiceFormatterTest, FormatsAnInvoiceWithNoLineItems) {
    const Invoice invoice{"INV-9", "Example Customer", {}};
    const Totals totals{0, 0};

    const std::string rendered = InvoiceFormatter::format(invoice, totals);

    EXPECT_NE(rendered.find("Subtotal: $0.00"), std::string::npos);
    EXPECT_NE(rendered.find("Total:    $0.00"), std::string::npos);
}

}  // namespace
}  // namespace billing
