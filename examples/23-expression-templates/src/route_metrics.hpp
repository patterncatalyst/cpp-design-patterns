#pragma once

#include <array>
#include <cstddef>

#include "construction_tracking.hpp"

namespace exprtmpl {

// Expression templates: a leg of a multi-leg freight route carries three
// physical metrics (distance covered, fuel burned, dwell time at the
// handoff dock). A dispatcher routinely needs a weighted combination of
// several legs for a route report, and wants that combination computed in
// a single pass with no intermediate RouteMetrics materialized along the
// way.
//
// RouteExpr is the CRTP base every node in an expression tree derives
// from. It supplies exactly one thing, a typed cast back to the concrete
// node, so operator+ and operator* below can accept "anything that is a
// RouteExpr" without knowing in advance whether that thing is a
// RouteMetrics leaf, a RouteSum, or a RouteScaled.
template <typename Derived>
class RouteExpr {
public:
    const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

// A node for "left plus right." Holding lhs_ and rhs_ by reference, not by
// value, is what keeps a RouteSum cheap to build: it never copies the
// operands it was handed, it only remembers where they are.
template <typename Left, typename Right>
class RouteSum : public RouteExpr<RouteSum<Left, Right>> {
public:
    RouteSum(const Left& lhs, const Right& rhs) : lhs_(lhs), rhs_(rhs) {}

    double operator[](std::size_t index) const { return lhs_[index] + rhs_[index]; }

private:
    const Left& lhs_;
    const Right& rhs_;
};

// A node for "factor times expr." Same shape as RouteSum: a scalar and a
// reference to the expression being scaled, nothing evaluated yet.
template <typename Expr>
class RouteScaled : public RouteExpr<RouteScaled<Expr>> {
public:
    RouteScaled(double factor, const Expr& expr) : factor_(factor), expr_(expr) {}

    double operator[](std::size_t index) const { return factor_ * expr_[index]; }

private:
    double factor_;
    const Expr& expr_;
};

template <typename Left, typename Right>
RouteSum<Left, Right> operator+(const RouteExpr<Left>& lhs, const RouteExpr<Right>& rhs) {
    return RouteSum<Left, Right>(lhs.derived(), rhs.derived());
}

template <typename Expr>
RouteScaled<Expr> operator*(double factor, const RouteExpr<Expr>& expr) {
    return RouteScaled<Expr>(factor, expr.derived());
}

// RouteMetrics plays two roles at once: it is a leaf expression (it
// derives from RouteExpr<RouteMetrics> and supplies its own operator[]),
// and it is the sink every expression tree eventually evaluates into. The
// templated converting constructor below is the ONLY place a full
// RouteMetrics gets built from an expression tree, no matter how many '+'
// and scalar '*' nodes that tree nests. Every intermediate RouteSum or
// RouteScaled node is a small reference-holding struct, never a
// RouteMetrics, so it never touches the construction counter.
class RouteMetrics : public RouteExpr<RouteMetrics> {
public:
    RouteMetrics(double distance_km, double fuel_liters, double dwell_hours);

    template <typename Expr>
    RouteMetrics(const RouteExpr<Expr>& expr)  // NOLINT(google-explicit-constructor)
        : values_{expr.derived()[0], expr.derived()[1], expr.derived()[2]} {
        ++g_construction_count;
    }

    double operator[](std::size_t index) const { return values_[index]; }

    double distance_km() const { return values_[0]; }
    double fuel_liters() const { return values_[1]; }
    double dwell_hours() const { return values_[2]; }

private:
    std::array<double, 3> values_;
};

}  // namespace exprtmpl
