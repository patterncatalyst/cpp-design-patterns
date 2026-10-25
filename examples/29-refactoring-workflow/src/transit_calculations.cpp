#include "transit_calculations.hpp"

namespace refactor29 {

int base_transit_days(const std::string& carrier,
                       const std::string& origin_zone,
                       const std::string& destination_zone,
                       double declared_value) {
    int days = 0;
    if (carrier == "ground") {
        days = (origin_zone == destination_zone) ? 2 : 5;
    } else if (carrier == "air") {
        days = (origin_zone == destination_zone) ? 1 : 2;
    } else if (carrier == "express") {
        days = 1;
    } else {
        days = 5;  // unrecognized carrier: ground-equivalent default
    }

    if (declared_value > 1000.0) {
        days += 1;  // high-value shipments require a delivery signature
    }
    return days;
}

double base_surcharge(double weight_kg, bool is_residential, double declared_value) {
    double surcharge = 0.0;
    if (weight_kg > 40.0) {
        surcharge += 15.0;
    }
    if (is_residential) {
        surcharge += 5.0;
    }
    if (declared_value > 1000.0) {
        surcharge += 12.50;
    }
    return surcharge;
}

}  // namespace refactor29
