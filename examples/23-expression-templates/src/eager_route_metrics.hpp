#pragma once

#include <array>
#include <cstddef>

namespace exprtmpl {

// The "write it the obvious way first" baseline this chapter contrasts
// expression templates against. operator+ and operator* each return a
// brand new NaiveRouteMetrics by value, so a chain of several operators
// materializes one intermediate object per operator, not one object for
// the whole expression. Same fields, same math as RouteMetrics in
// route_metrics.hpp; only the evaluation strategy differs.
class NaiveRouteMetrics {
public:
    NaiveRouteMetrics(double distance_km, double fuel_liters, double dwell_hours);

    double distance_km() const { return values_[0]; }
    double fuel_liters() const { return values_[1]; }
    double dwell_hours() const { return values_[2]; }

private:
    std::array<double, 3> values_;
};

NaiveRouteMetrics operator+(const NaiveRouteMetrics& lhs, const NaiveRouteMetrics& rhs);
NaiveRouteMetrics operator*(double factor, const NaiveRouteMetrics& rhs);

}  // namespace exprtmpl
