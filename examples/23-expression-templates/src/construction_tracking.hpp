#pragma once

#include <cstddef>

namespace exprtmpl {

// Shared counter the gtest suite uses to prove the expression-template
// path constructs exactly one result value while the eager path
// constructs one intermediate value per arithmetic operation. Both
// RouteMetrics (lazy, see route_metrics.hpp) and NaiveRouteMetrics (eager,
// see eager_route_metrics.hpp) increment this same counter from their
// value-building constructors; nothing else touches it.
inline std::size_t g_construction_count = 0;

inline void reset_construction_count() { g_construction_count = 0; }
inline std::size_t construction_count() { return g_construction_count; }

}  // namespace exprtmpl
