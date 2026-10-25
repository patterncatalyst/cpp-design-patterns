#include "shipment_eta_scheduler.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace breakdep28 {
namespace {

// The C++20 test subclass: Subclass and Override Method applied to the one
// call Extract and Override Call isolated.
class FixedClockEtaScheduler : public ShipmentEtaScheduler {
public:
    FixedClockEtaScheduler(int transit_days, std::chrono::system_clock::time_point fixed_now)
        : ShipmentEtaScheduler(transit_days), fixed_now_(fixed_now) {}

protected:
    std::chrono::system_clock::time_point current_time() const override { return fixed_now_; }

private:
    std::chrono::system_clock::time_point fixed_now_;
};

TEST(ShipmentEtaSchedulerTest, ProductionPathReadsTheRealClock) {
    ShipmentEtaScheduler scheduler(3);

    const auto before = std::chrono::system_clock::now();
    const auto result = scheduler.schedule_shipment("SH-9001");
    const auto after = std::chrono::system_clock::now();

    EXPECT_GE(result.dispatched_at, before);
    EXPECT_LE(result.dispatched_at, after);
}

TEST(ShipmentEtaSchedulerTest, TestSubclassOverridesTheClockForADeterministicEta) {
    const auto fixed_now = std::chrono::sys_days{std::chrono::year{2026} / 3 / 1};
    FixedClockEtaScheduler scheduler(5, fixed_now);

    const auto result = scheduler.schedule_shipment("SH-9002");

    EXPECT_EQ(result.dispatched_at, fixed_now);
    EXPECT_EQ(result.promised_by, fixed_now + std::chrono::days(5));
}

#if __cplusplus >= 202302L

// The C++23 counterpart: no virtual, no override, just a derived class
// whose own current_time() hides the base one once Self deduces to it.
class FixedClockDeducingEtaScheduler : public DeducingEtaScheduler {
public:
    FixedClockDeducingEtaScheduler(int transit_days, std::chrono::system_clock::time_point fixed_now)
        : DeducingEtaScheduler(transit_days), fixed_now_(fixed_now) {}

    std::chrono::system_clock::time_point current_time() const { return fixed_now_; }

private:
    std::chrono::system_clock::time_point fixed_now_;
};

TEST(ShipmentEtaSchedulerTest, DeducingThisVariantSubstitutesTheClockUnderCpp23) {
    const auto fixed_now = std::chrono::sys_days{std::chrono::year{2026} / 3 / 1};
    FixedClockDeducingEtaScheduler scheduler(5, fixed_now);

    const auto result = scheduler.schedule_shipment("SH-9003");

    EXPECT_EQ(result.dispatched_at, fixed_now);
    EXPECT_EQ(result.promised_by, fixed_now + std::chrono::days(5));
}

#endif

}  // namespace
}  // namespace breakdep28
