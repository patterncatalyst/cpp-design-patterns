#include "invoice_calculator.hpp"

#include <gtest/gtest.h>

#include "invoice_rules.hpp"

namespace billing {
namespace {

Invoice make_invoice() {
    return Invoice{
        "INV-1",
        "Example Customer",
        {
            {"Website design", 1, 250000},
            {"Hosting (monthly)", 3, 1500},
        },
    };
}

TEST(InvoiceCalculatorTest, SumsLineItemsIntoTheSubtotalWithNoRules) {
    const InvoiceCalculator calculator({});

    const Totals totals = calculator.totals_for(make_invoice());

    EXPECT_EQ(totals.subtotal, 254500);
    EXPECT_EQ(totals.total, 254500);
}

TEST(InvoiceCalculatorTest, AppliesEveryRuleToTheOriginalSubtotal) {
    const PercentageTaxRule sales_tax(0.0825);
    const PercentageDiscountRule discount(0.10);
    const FlatServiceFeeRule service_fee(2500);
    const InvoiceCalculator calculator({sales_tax, discount, service_fee});

    const Totals totals = calculator.totals_for(make_invoice());

    EXPECT_EQ(totals.subtotal, 254500);
    EXPECT_EQ(totals.total, 252546);
}

TEST(PercentageTaxRuleTest, ComputesAnExactPercentageOfTheSubtotal) {
    const PercentageTaxRule sales_tax(0.0825);

    EXPECT_EQ(sales_tax.adjustment_for(254500), 20996);
}

TEST(PercentageDiscountRuleTest, ClampsAnOutOfRangeRateInsteadOfOverdiscounting) {
    const PercentageDiscountRule runaway_discount(1.5);  // 150% requested at construction

    // Clamped to 100%: the adjustment can discount the whole subtotal, but
    // never more than it, honoring the InvoiceRule contract even when a
    // caller passes a rate outside the documented range.
    EXPECT_EQ(runaway_discount.adjustment_for(100000), -100000);
}

// --- Open/Closed, exercised directly: a brand-new rule type, defined only
// in this test file, joins the calculator with zero changes to
// InvoiceCalculator, InvoiceRule, or any rule declared in
// invoice_rules.hpp. If OCP held, this is all adding a new rule kind ever
// takes. ---
class RoundUpToWholeDollarRule : public InvoiceRule {
public:
    Cents adjustment_for(Cents subtotal) const override {
        const Cents remainder = subtotal % 100;
        return remainder == 0 ? 0 : (100 - remainder);
    }

    std::string_view name() const override { return "round up to whole dollar"; }
};

TEST(InvoiceCalculatorTest, AcceptsANewRuleTypeDefinedOutsideProductionCode) {
    const RoundUpToWholeDollarRule round_up;
    Invoice invoice = make_invoice();
    invoice.line_items.push_back({"Rush fee", 1, 33});  // pushes the subtotal to 254533

    const InvoiceCalculator calculator({round_up});
    const Totals totals = calculator.totals_for(invoice);

    EXPECT_EQ(totals.subtotal, 254533);
    EXPECT_EQ(totals.total, 254600);  // rounded up to the next whole dollar
}

#if __cplusplus >= 202302L
TEST(InvoiceCalculatorTest, TryTotalsForRejectsAnInvoiceWithNoLineItems) {
    const InvoiceCalculator calculator({});
    const Invoice empty_invoice{"INV-0", "Nobody", {}};

    const auto result = calculator.try_totals_for(empty_invoice);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "invoice has no line items to total");
}

TEST(InvoiceCalculatorTest, TryTotalsForReturnsTotalsForAValidInvoice) {
    const InvoiceCalculator calculator({});

    const auto result = calculator.try_totals_for(make_invoice());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->total, 254500);
}
#endif

}  // namespace
}  // namespace billing
