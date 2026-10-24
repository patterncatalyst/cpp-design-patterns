#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace freight {

// Destination service zone a shipment is quoted into. Unassigned is the
// default a request arrives with before an address lookup resolves it, and
// is never a valid zone to quote against.
enum class ServiceZone {
    Local,
    Regional,
    National,
    International,
    Unassigned,
};

// Money in this domain is tracked as integer cents, never floating point,
// so a quote is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// The maximum declared value this carrier network will insure. A request
// above this line is rejected at intake rather than quoted and recorded.
inline constexpr Cents kMaxInsurableValueCents = 5'000'000;  // $50,000.00

// A shipment as it arrives at intake, before validation or pricing.
struct ShipmentRequest {
    double weight_kg;
    ServiceZone zone;
    Cents declared_value_cents;
};

// Every way intake can dispose of a ShipmentRequest. Accepted is the only
// outcome that goes on to be priced and recorded.
enum class ValidationOutcome {
    Accepted,
    RejectedNegativeWeight,
    RejectedMissingZone,
    RejectedDeclaredValueTooHigh,
};

// Checks a request against every intake rule and reports which one, if
// any, it failed. Pure function, no side effects, which is what makes it a
// clean fit for a parameterized test over a table of cases.
ValidationOutcome validate_shipment(const ShipmentRequest& request);

// A short, human-readable label for an outcome, used by the demo binary
// only; the test suite asserts on the enum value itself, never on this
// string.
std::string_view describe(ValidationOutcome outcome);

// FREIGHT_REQUIRE checks a precondition unconditionally, in every build
// configuration. The standard library's assert() would be the more
// familiar choice, but this book's CMake presets build every example as
// Release (see CMakePresets.json), and CMake's default Release flags add
// -DNDEBUG, which compiles assert() down to nothing. A precondition that
// matters, like "never price a shipment with a negative weight," has to
// stay checked everywhere this binary ships, not only in a debug build
// nobody is running in CI. quote_cost() below uses this macro instead of
// assert() for exactly that reason, and the death test later in this
// chapter depends on it firing.
#define FREIGHT_REQUIRE(condition, message)                                  \
    do {                                                                     \
        if (!(condition)) {                                                 \
            std::fprintf(stderr, "FREIGHT_REQUIRE failed: %s\n", (message)); \
            std::abort();                                                   \
        }                                                                    \
    } while (false)

// Prices a shipment. This function has a precondition its caller must
// satisfy: weight_kg must already be non-negative, which
// validate_shipment() above checks before intake ever calls this. Calling
// it directly with a malformed request, the way a death test deliberately
// does, aborts the process instead of returning a nonsense price.
Cents quote_cost(const ShipmentRequest& request);

}  // namespace freight
