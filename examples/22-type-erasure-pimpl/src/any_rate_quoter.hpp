#pragma once

#include <concepts>
#include <memory>
#include <utility>

#include "shipping_domain.hpp"

namespace shipping {

// Quotable is the structural requirement AnyRateQuoter erases behind: any
// type exposing a quote(const Shipment&) member convertible to Cents
// satisfies it. There is no shared base class anywhere in this
// constraint, and no inheritance relationship declared up front the way
// Chapter 8's RateStrategy interface required. A type either has the
// right shape or it does not, checked at the point AnyRateQuoter is
// constructed from it.
template <typename Quoter>
concept Quotable = requires(const Quoter& quoter, const Shipment& shipment) {
    { quoter.quote(shipment) } -> std::convertible_to<Cents>;
};

// AnyRateQuoter is a value-semantic wrapper that can hold ANY type
// satisfying Quotable, with no shared base class, dispatching through a
// hand-written concept/model pair: Concept is the erased interface this
// class dispatches through, and Model<Quoter> is a template that adapts
// one concrete Quoter to it. This is the same technique std::function and
// std::any use internally to hold an unbounded family of unrelated types
// behind one fixed-size value.
//
// Concept and Model are fully visible right here in this header, nested
// private to AnyRateQuoter. That is a deliberate contrast with the PImpl
// idiom this chapter also covers: PImpl hides its Impl across a
// translation-unit boundary, in a separate .cpp file the header never
// sees, which is why it needs hand-written special members. AnyRateQuoter
// hides Concept and Model from *callers* (they are private), not from the
// compiler composing this header, so its destructor and move members can
// stay defaulted.
class AnyRateQuoter {
public:
    // Intentionally non-explicit: any Quotable value converts into an
    // AnyRateQuoter the same way any compatible callable converts into a
    // std::function, with no need to name AnyRateQuoter at the call site.
    template <Quotable Quoter>
    AnyRateQuoter(Quoter quoter) : model_(std::make_unique<Model<Quoter>>(std::move(quoter))) {}

    AnyRateQuoter(const AnyRateQuoter& other) : model_(other.model_->clone()) {}

    AnyRateQuoter& operator=(const AnyRateQuoter& other) {
        if (this != &other) {
            model_ = other.model_->clone();
        }
        return *this;
    }

    AnyRateQuoter(AnyRateQuoter&&) noexcept = default;
    AnyRateQuoter& operator=(AnyRateQuoter&&) noexcept = default;
    ~AnyRateQuoter() = default;

    Cents quote(const Shipment& shipment) const { return model_->quote(shipment); }

private:
    // The erased interface: every Model<Quoter>, regardless of what
    // concrete Quoter it adapts, is reachable through exactly this shape.
    struct Concept {
        virtual ~Concept() = default;
        virtual Cents quote(const Shipment& shipment) const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    // One Model<Quoter> instantiation per distinct Quoter type held by any
    // AnyRateQuoter anywhere in the program; each holds its Quoter by
    // value and forwards quote() and clone() to it.
    template <Quotable Quoter>
    struct Model : Concept {
        explicit Model(Quoter quoter) : quoter_(std::move(quoter)) {}

        Cents quote(const Shipment& shipment) const override { return quoter_.quote(shipment); }

        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(quoter_); }

        Quoter quoter_;
    };

    std::unique_ptr<Concept> model_;
};

}  // namespace shipping
