#pragma once

#include <string>
#include <vector>

namespace palletizing {

// A pallet or carton reports an overweight condition through this
// interface. The warehouse floor cares about overweight events, but not
// every caller of PackageGroup::check_weight_limit() does, and the ones
// that don't should never have to special-case the absence of a handler.
class ShipmentExceptionHandler {
public:
    virtual ~ShipmentExceptionHandler() = default;
    virtual void on_overweight(const std::string& group_label, int grams_over_limit) const = 0;
};

// The Null Object: a do-nothing ShipmentExceptionHandler. PackageGroup binds
// every instance to a real handler reference at construction, defaulting to
// this one when the caller doesn't supply its own, so PackageGroup's own
// code never holds a null pointer and never writes
// `if (handler_) { handler_->on_overweight(...); }`. It calls
// handler_.on_overweight(...) unconditionally, and that call is either real
// work or a no-op depending on which handler got bound, never a branch.
class NullShipmentExceptionHandler : public ShipmentExceptionHandler {
public:
    void on_overweight(const std::string& group_label, int grams_over_limit) const override;
};

// Returns a reference to one process-wide NullShipmentExceptionHandler
// instance. A function-local static has the same lifetime as a
// Meyer's-singleton: constructed on first use, destroyed at program exit,
// safe to bind a long-lived PackageGroup's handler_ reference to.
const ShipmentExceptionHandler& null_shipment_exception_handler();

// A real handler for the demo and the test suite: records every overweight
// event instead of acting on it, so a test can assert on what the handler
// was told without standing up a logging framework.
class LoggingShipmentExceptionHandler : public ShipmentExceptionHandler {
public:
    void on_overweight(const std::string& group_label, int grams_over_limit) const override;

    const std::vector<std::string>& events() const { return events_; }

private:
    // mutable because on_overweight() is logically const from a caller's
    // point of view (it reports a fact, it does not change the handler's
    // identity), but recording that fact still has to write somewhere.
    mutable std::vector<std::string> events_;
};

}  // namespace palletizing
