#pragma once

#include <string>

#include "order_domain.hpp"

namespace orderexport {

// A narrower contrast than OrderExportPipeline above: one hook, dispatched
// statically instead of through a vtable. Neither class here replaces
// OrderExportPipeline; they exist to show what changes, and what does
// not, when a Template Method's hook is resolved at compile time rather
// than at runtime.

// C++20: the Curiously Recurring Template Pattern. The base class itself
// must be a template parameterized on the derived type, and the one step
// that needs the derived type's hook casts *this down to Derived
// explicitly. Every concrete summary has to name itself as the template
// argument to its own base class.
template <typename Derived>
class CrtpSummarySkeleton {
public:
    std::string summarize(const Order& order) const {
        return static_cast<const Derived&>(*this).describe(order);
    }
};

class CrtpOrderSummary : public CrtpSummarySkeleton<CrtpOrderSummary> {
public:
    std::string describe(const Order& order) const {
        return order.order_id + ": " + std::to_string(order.lines.size()) + " line(s)";
    }
};

#if __cplusplus >= 202302L
// C++23: deducing this. The base class is an ordinary, non-template
// class; summarize() takes an explicit object parameter instead of the
// implicit *this, and the compiler deduces Self as whatever the
// most-derived type is at the call site. No CRTP template argument, and
// no static_cast, anywhere in this version.
class DeducingThisSummarySkeleton {
public:
    template <typename Self>
    std::string summarize(this const Self& self, const Order& order) {
        return self.describe(order);
    }
};

class DeducedOrderSummary : public DeducingThisSummarySkeleton {
public:
    std::string describe(const Order& order) const {
        return order.order_id + ": " + std::to_string(order.lines.size()) + " line(s)";
    }
};
#endif

}  // namespace orderexport
