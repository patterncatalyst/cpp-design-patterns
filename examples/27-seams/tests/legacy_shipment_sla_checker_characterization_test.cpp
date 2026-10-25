#include "legacy_shipment_sla_checker.hpp"

#include <chrono>

#include <gtest/gtest.h>

#include "shipment_sla.hpp"

namespace seam27 {
namespace {

// These are characterization tests, not specification tests: they pin
// what LegacyShipmentSlaChecker actually does today, observed by running
// it, not what its contract should be in the abstract. Both margins below
// are two hours wide specifically so the handful of milliseconds between
// this test computing "now" and is_breached() reading the real wall clock
// a moment later can never flip either answer; that margin is also the
// tell that something is wrong with how this class is built. A
// characterization test is not supposed to need a safety margin to avoid
// flakiness. It needs one here only because the class under test has no
// seam, so the test is forced to reason about the real clock instead of a
// clock it controls.
//
// The one case this file cannot write is a deadline exactly equal to
// "now": there is no way to make std::chrono::system_clock::now(), called
// once inside this test and once inside is_breached(), land on the same
// instant on purpose. That gap in coverage is not an oversight. It is the
// concrete cost of the hard dependency, and it is exactly what
// shipment_sla_checker_test.cpp closes once the object seam exists.

TEST(LegacyShipmentSlaCheckerCharacterizationTest, DeadlineWellInThePastIsBreached) {
    LegacyShipmentSlaChecker checker;
    const ShipmentSla sla{"SHM-9001", std::chrono::system_clock::now() - std::chrono::hours(2)};

    EXPECT_TRUE(checker.is_breached(sla));
}

TEST(LegacyShipmentSlaCheckerCharacterizationTest, DeadlineWellInTheFutureIsNotBreached) {
    LegacyShipmentSlaChecker checker;
    const ShipmentSla sla{"SHM-9002", std::chrono::system_clock::now() + std::chrono::hours(2)};

    EXPECT_FALSE(checker.is_breached(sla));
}

}  // namespace
}  // namespace seam27
