#include "eager_route_metrics.hpp"

#include "construction_tracking.hpp"

namespace exprtmpl {

NaiveRouteMetrics::NaiveRouteMetrics(double distance_km, double fuel_liters, double dwell_hours)
    : values_{distance_km, fuel_liters, dwell_hours} {
    ++g_construction_count;
}

NaiveRouteMetrics operator+(const NaiveRouteMetrics& lhs, const NaiveRouteMetrics& rhs) {
    return NaiveRouteMetrics(lhs.distance_km() + rhs.distance_km(), lhs.fuel_liters() + rhs.fuel_liters(),
                              lhs.dwell_hours() + rhs.dwell_hours());
}

NaiveRouteMetrics operator*(double factor, const NaiveRouteMetrics& rhs) {
    return NaiveRouteMetrics(factor * rhs.distance_km(), factor * rhs.fuel_liters(), factor * rhs.dwell_hours());
}

}  // namespace exprtmpl
