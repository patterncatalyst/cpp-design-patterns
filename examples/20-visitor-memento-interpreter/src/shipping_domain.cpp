#include "shipping_domain.hpp"

#include <iomanip>
#include <sstream>

namespace shipping {

std::string zone_name(DestinationZone zone) {
    switch (zone) {
        case DestinationZone::Local:
            return "Local";
        case DestinationZone::Regional:
            return "Regional";
        case DestinationZone::National:
            return "National";
        case DestinationZone::International:
            return "International";
    }
    return "Unknown";
}

std::string format_weight_kg(double weight_kg) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << weight_kg << "kg";
    return out.str();
}

std::string format_cents(Cents cents) {
    std::ostringstream out;
    out << '$' << std::fixed << std::setprecision(2) << (static_cast<double>(cents) / 100.0);
    return out.str();
}

}  // namespace shipping
