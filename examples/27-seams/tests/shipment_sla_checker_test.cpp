#include "shipment_sla_checker.hpp"

#include <chrono>

#include <gtest/gtest.h>

#include "clock.hpp"
#include "mock_clock.hpp"
#include "shipment_sla.hpp"

namespace seam27 {
namespace {

using ::testing::Return;

// These two tests use the real SystemClock, not a mock, specifically to
// prove that extracting the Clock interface and injecting it through the
// constructor changed nothing observable at the wide margins
// LegacyShipmentSlaChecker's characterization tests already pinned. A
// refactor that introduces a seam is only trustworthy if the behavior it
// touches stays identical at every point the old tests already covered.

TEST(ShipmentSlaCheckerTest, SeamAloneDoesNotChangeTheWideMarginBehaviorTheLegacyCheckerHad) {
    SystemClock clock;
    ShipmentSlaChecker checker(clock);
    const ShipmentSla overdue{"SHM-9101", std::chrono::system_clock::now() - std::chrono::hours(2)};
    const ShipmentSla pending{"SHM-9102", std::chrono::system_clock::now() + std::chrono::hours(2)};

    EXPECT_TRUE(checker.is_breached(overdue));
    EXPECT_FALSE(checker.is_breached(pending));
}

// With a substitutable Clock in place, the exact boundary the legacy
// checker could never be tested against becomes two ordinary, completely
// deterministic tests. ShipmentSlaChecker treats a deadline exactly equal
// to "now" as breached (>=), the corrected rule chosen once the seam made
// the boundary visible at all; the legacy checker's own >= comparison is
// never touched, by design, because the historical checker is kept as the
// pinned record of what the inherited code did before anyone changed it.

TEST(ShipmentSlaCheckerTest, DeadlineExactlyAtNowIsBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9103", deadline};

    EXPECT_TRUE(checker.is_breached(sla));
}

TEST(ShipmentSlaCheckerTest, OneNanosecondBeforeTheDeadlineIsNotBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline - std::chrono::nanoseconds(1)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9104", deadline};

    EXPECT_FALSE(checker.is_breached(sla));
}

TEST(ShipmentSlaCheckerTest, OneNanosecondAfterTheDeadlineIsBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline + std::chrono::nanoseconds(1)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9105", deadline};

    EXPECT_TRUE(checker.is_breached(sla));
}

#if __cplusplus >= 202302L

TEST(ShipmentSlaCheckerTest, TryTimeRemainingReturnsTheMinutesLeftWhenNotYetBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline - std::chrono::minutes(10)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9106", deadline};

    const auto result = checker.try_time_remaining(sla);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), std::chrono::minutes(10));
}

TEST(ShipmentSlaCheckerTest, TryTimeRemainingCarriesHowManyMinutesOverdueWhenBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline + std::chrono::minutes(7)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9107", deadline};

    const auto result = checker.try_time_remaining(sla);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::chrono::minutes(7));
}

#else

TEST(ShipmentSlaCheckerTest, TryTimeRemainingReturnsTheMinutesLeftWhenNotYetBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline - std::chrono::minutes(10)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9106", deadline};

    const auto result = checker.try_time_remaining(sla);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), std::chrono::minutes(10));
}

TEST(ShipmentSlaCheckerTest, TryTimeRemainingReturnsNulloptOnceBreached) {
    const auto deadline = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};
    MockClock clock;
    EXPECT_CALL(clock, now()).WillOnce(Return(deadline + std::chrono::minutes(7)));
    ShipmentSlaChecker checker(clock);
    const ShipmentSla sla{"SHM-9107", deadline};

    const auto result = checker.try_time_remaining(sla);

    EXPECT_FALSE(result.has_value());
}

#endif

}  // namespace
}  // namespace seam27
