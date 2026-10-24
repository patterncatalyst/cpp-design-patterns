#pragma once

#include <cstdint>
#include <string>

namespace shipping {

// Destination zone a ZoneIs predicate can match against. Shared by every
// pattern in this example so a rule built with one pattern's node shape
// means exactly the same thing when read by another.
enum class DestinationZone {
    Local,
    Regional,
    National,
    International,
};

// Money in this domain is tracked as integer cents, never floating point,
// so a rule threshold comparison is exact and reproducible in a test
// assertion, the same convention Chapter 8's shipping_domain.hpp set.
using Cents = std::int64_t;

// The shipment a rule-node AST is evaluated against, the same small shape
// this book has used since Chapter 8's Strategy.
struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};

// Shared formatting helpers so Visitor's classic and variant describe()
// operations, and the Interpreter's printed output, render a zone, a
// weight, and a money amount identically no matter which pattern asked.
std::string zone_name(DestinationZone zone);
std::string format_weight_kg(double weight_kg);
std::string format_cents(Cents cents);

}  // namespace shipping
