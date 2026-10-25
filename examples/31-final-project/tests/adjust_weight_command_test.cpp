#include "adjust_weight_command.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "shipment.hpp"
#include "weight_tiered_rate_strategy.hpp"

namespace finalproject31 {
namespace {

WeightTieredRateStrategy make_strategy() {
    return WeightTieredRateStrategy({
        WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
        WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
    });
}

TEST(AdjustWeightCommandTest, ExecuteSetsTheNewWeightAndRecomputesTheQuoteViaTheStrategy) {
    WeightTieredRateStrategy strategy = make_strategy();
    Shipment shipment("SHIP-1", "Skyhaul Express", "NATIONAL", 3.0, strategy.quote(Shipment("tmp", "", "", 3.0, 0)));
    AdjustWeightCommand command(shipment, strategy, 15.0);

    command.execute();

    EXPECT_DOUBLE_EQ(shipment.weight_kg(), 15.0);
    // Command invoking Strategy: the new quote is whatever the same
    // strategy computes for the new weight, not a value the command made
    // up on its own.
    EXPECT_EQ(shipment.quoted_rate_cents(), strategy.quote(shipment));
}

TEST(AdjustWeightCommandTest, UndoRestoresThePriorWeightAndQuoteExactly) {
    WeightTieredRateStrategy strategy = make_strategy();
    const double original_weight = 3.0;
    const Cents original_quote = strategy.quote(Shipment("tmp", "", "", original_weight, 0));
    Shipment shipment("SHIP-1", "Skyhaul Express", "NATIONAL", original_weight, original_quote);
    AdjustWeightCommand command(shipment, strategy, 15.0);

    command.execute();
    command.undo();

    EXPECT_DOUBLE_EQ(shipment.weight_kg(), original_weight);
    EXPECT_EQ(shipment.quoted_rate_cents(), original_quote);
}

TEST(AdjustWeightCommandTest, UndoBeforeExecuteThrows) {
    WeightTieredRateStrategy strategy = make_strategy();
    Shipment shipment("SHIP-1", "Skyhaul Express", "NATIONAL", 3.0, 0);
    AdjustWeightCommand command(shipment, strategy, 15.0);

    EXPECT_THROW(command.undo(), std::logic_error);
}

TEST(AdjustWeightCommandTest, DescriptionNamesTheShipmentAndTargetWeight) {
    WeightTieredRateStrategy strategy = make_strategy();
    Shipment shipment("SHIP-9001", "Skyhaul Express", "NATIONAL", 3.0, 0);
    AdjustWeightCommand command(shipment, strategy, 15.0);

    EXPECT_NE(command.description().find("SHIP-9001"), std::string::npos);
}

}  // namespace
}  // namespace finalproject31
