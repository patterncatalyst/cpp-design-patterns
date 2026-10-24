#pragma once

#include <cstdint>

namespace crtp21 {

// Money in this domain is tracked as integer cents, never floating point,
// the same convention examples/08-strategy and examples/09-template-method
// use, so every quote this example computes (including the compile-time
// ones) is exact and reproducible in a static_assert, not just a test.
using Cents = std::int64_t;

// A parcel ready to be quoted. Deliberately an aggregate with only the two
// fields this chapter's rate models and policies need, so a Shipment can be
// built as a constexpr value and fed through a compile-time quote.
struct Shipment {
    double weight_kg;
    Cents declared_value_cents;
};

}  // namespace crtp21
