#pragma once

#include <string_view>

#include "billing_domain.hpp"

namespace billing {

// The abstraction OCP extends instead of editing: anything that can turn a
// subtotal into an adjustment is an InvoiceRule, and InvoiceCalculator
// (invoice_calculator.hpp) never has to change to pick up a new one.
//
// Contract every InvoiceRule must honor: for a non-negative subtotal, the
// returned adjustment must satisfy -subtotal <= adjustment, so applying any
// rule, or any number of rules, can never drive an invoice's running total
// negative. adjustment_for must also be a pure function of the subtotal it
// is given, with no hidden state read from anywhere else, so a calculator
// can apply rules in any order without surprising a caller.
class InvoiceRule {
public:
    virtual ~InvoiceRule() = default;
    virtual Cents adjustment_for(Cents subtotal) const = 0;
    virtual std::string_view name() const = 0;
};

// A sales tax rule: always adds, never exceeds the subtotal. rate is
// clamped to [0, 1] so a misconfigured rate cannot violate the InvoiceRule
// contract above.
class PercentageTaxRule : public InvoiceRule {
public:
    explicit PercentageTaxRule(double rate);
    Cents adjustment_for(Cents subtotal) const override;
    std::string_view name() const override;

private:
    double rate_;
};

// A percentage discount. adjustment_for is declared final: it always
// clamps its rate to [0, 1] and applies it here, once, so every subtype
// below inherits the invariant "never discounts more than the clamped rate
// times the subtotal" instead of having to reimplement it.
class PercentageDiscountRule : public InvoiceRule {
public:
    explicit PercentageDiscountRule(double rate);
    Cents adjustment_for(Cents subtotal) const final;
    std::string_view name() const override;

protected:
    // The one hook a subtype overrides to change which rate applies; the
    // clamp and the sign of the adjustment stay fixed in the base class, so
    // no subtype can accidentally return a positive adjustment or discount
    // past the subtotal.
    virtual double discount_rate(Cents subtotal) const;

private:
    double rate_;
};

// A refinement of PercentageDiscountRule for repeat customers: the same
// contract, substitutable anywhere a PercentageDiscountRule or an
// InvoiceRule is expected, because it only overrides discount_rate() and
// leaves adjustment_for()'s clamp alone. A loyalty bonus of 1% per year,
// capped at 10 years, is added to the base rate, then the combined rate is
// re-clamped by the parent before it is ever applied to a subtotal.
class LoyaltyDiscountRule : public PercentageDiscountRule {
public:
    LoyaltyDiscountRule(double base_rate, int loyalty_years);
    std::string_view name() const override;

protected:
    double discount_rate(Cents subtotal) const override;

private:
    double bonus_rate_;
};

// A flat service fee: a fixed addition with no relationship to the
// subtotal at all. Added after PercentageTaxRule and PercentageDiscountRule
// already existed, with zero changes to InvoiceCalculator or to either of
// them: the OCP demonstration this chapter's test suite exercises further
// with a rule type defined entirely outside this header.
class FlatServiceFeeRule : public InvoiceRule {
public:
    explicit FlatServiceFeeRule(Cents flat_cents);
    Cents adjustment_for(Cents subtotal) const override;
    std::string_view name() const override;

private:
    Cents flat_cents_;
};

}  // namespace billing
