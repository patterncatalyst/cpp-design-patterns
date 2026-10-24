#include "stochastic_fsm.hpp"

#include <gtest/gtest.h>

#include <random>
#include <vector>

namespace shipment {
namespace {

TEST(StochasticShipmentMachineTest, DeterministicEventsMatchTheTableRules) {
    StochasticShipmentMachine machine(std::mt19937(1));

    machine.pay();
    EXPECT_EQ(machine.status(), ShipmentStatus::Paid);

    machine.ship();
    EXPECT_EQ(machine.status(), ShipmentStatus::InTransit);
}

TEST(StochasticShipmentMachineTest, CancelBeforeShippingIsLegal) {
    StochasticShipmentMachine machine(std::mt19937(1));

    machine.pay();
    machine.cancel();

    EXPECT_EQ(machine.status(), ShipmentStatus::Cancelled);
}

TEST(StochasticShipmentMachineTest, ShipBeforePayIsRejected) {
    StochasticShipmentMachine machine(std::mt19937(1));
    EXPECT_THROW(machine.ship(), IllegalTransition);
}

TEST(StochasticShipmentMachineTest, AdvanceBeforeInTransitIsRejected) {
    StochasticShipmentMachine machine(std::mt19937(1));
    EXPECT_THROW(machine.advance(), IllegalTransition);
}

// A fixed seed must reproduce the exact same sequence of weighted outcomes
// every run. This is what makes the stochastic machine testable at all:
// without an injected, seedable engine, the sequence below would differ
// every time this test ran.
TEST(StochasticShipmentMachineTest, SeededRunProducesAnExactReproducibleSequence) {
    StochasticShipmentMachine machine(std::mt19937(38));
    machine.pay();
    machine.ship();

    const auto sequence = machine.run_to_terminal();

    const std::vector<ShipmentStatus> expected = {ShipmentStatus::InTransit, ShipmentStatus::InTransit,
                                                   ShipmentStatus::InTransit, ShipmentStatus::Delivered};
    EXPECT_EQ(sequence, expected);
}

TEST(StochasticShipmentMachineTest, SameSeedProducesTheSameSequenceTwice) {
    StochasticShipmentMachine first(std::mt19937(38));
    first.pay();
    first.ship();

    StochasticShipmentMachine second(std::mt19937(38));
    second.pay();
    second.ship();

    EXPECT_EQ(first.run_to_terminal(), second.run_to_terminal());
}

// Not a single draw: thousands of independent single-step trials, each
// from a freshly seeded machine, so the tally approximates the configured
// weights (0.85 / 0.10 / 0.05) rather than depending on one lucky seed.
TEST(StochasticShipmentMachineTest, EmpiricalDistributionApproximatesTheConfiguredWeights) {
    constexpr int kTrials = 20000;
    int delivered = 0;
    int delayed = 0;
    int lost = 0;

    std::mt19937 seed_source(7);
    for (int i = 0; i < kTrials; ++i) {
        StochasticShipmentMachine machine{std::mt19937(seed_source())};
        machine.pay();
        machine.ship();

        switch (machine.advance()) {
            case ShipmentStatus::Delivered: ++delivered; break;
            case ShipmentStatus::InTransit: ++delayed; break;
            case ShipmentStatus::Lost: ++lost; break;
            default: FAIL() << "advance() produced a status outside {Delivered, InTransit, Lost}";
        }
    }

    const double delivered_freq = static_cast<double>(delivered) / kTrials;
    const double delayed_freq = static_cast<double>(delayed) / kTrials;
    const double lost_freq = static_cast<double>(lost) / kTrials;

    EXPECT_NEAR(delivered_freq, 0.85, 0.02);
    EXPECT_NEAR(delayed_freq, 0.10, 0.02);
    EXPECT_NEAR(lost_freq, 0.05, 0.02);
}

}  // namespace
}  // namespace shipment
