#include "route_metrics.hpp"

namespace exprtmpl {

RouteMetrics::RouteMetrics(double distance_km, double fuel_liters, double dwell_hours)
    : values_{distance_km, fuel_liters, dwell_hours} {
    ++g_construction_count;
}

}  // namespace exprtmpl
