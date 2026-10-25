#pragma once

#include <chrono>

namespace refactor29 {

// The one result value every stage of this example returns: how many
// transit days a shipment needs, what it costs beyond the base rate, and
// the calendar date it lands on. All three estimators (Legacy, Seamed,
// Modern) hand back the same shape so a test can compare them directly.
struct EtaEstimate {
    int transit_days = 0;
    double surcharge = 0.0;
    std::chrono::year_month_day delivery_date{};
};

}  // namespace refactor29
