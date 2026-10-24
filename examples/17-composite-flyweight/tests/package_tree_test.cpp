#include "package_tree.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace palletizing {
namespace {

SkuCatalog make_catalog() {
    SkuCatalog catalog;
    catalog.register_sku({"WIDGET-100", "Standard widget", 250, 80, 80, 40, HazmatClass::None});
    catalog.register_sku({"GADGET-200", "Precision gadget", 400, 120, 90, 60, HazmatClass::None});
    catalog.register_sku({"BATTERY-300", "Lithium battery pack", 900, 150, 100, 70, HazmatClass::Lithium});
    return catalog;
}

TEST(ItemTest, WeightAndCountScaleWithQuantity) {
    SkuCatalog catalog = make_catalog();
    const Item item("line-1", catalog.get("WIDGET-100"), /*quantity=*/4, "Austin, TX");

    EXPECT_EQ(item.total_weight_grams(), 1000);
    EXPECT_EQ(item.item_count(), 4);
}

TEST(PackageGroupTest, SumsDirectLeafChildren) {
    SkuCatalog catalog = make_catalog();
    PackageGroup carton("Carton CTN-A");
    carton.add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 40, "Austin, TX"));
    carton.add(std::make_shared<Item>("line-2", catalog.get("GADGET-200"), 12, "Austin, TX"));

    EXPECT_EQ(carton.total_weight_grams(), 14800);
    EXPECT_EQ(carton.item_count(), 52);
}

TEST(PackageGroupTest, RecursesThroughAMixedTreeOfCartonsAndItems) {
    SkuCatalog catalog = make_catalog();

    auto carton_a = std::make_shared<PackageGroup>("Carton CTN-A");
    carton_a->add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 40, "Austin, TX"));
    carton_a->add(std::make_shared<Item>("line-2", catalog.get("GADGET-200"), 12, "Austin, TX"));

    auto carton_c = std::make_shared<PackageGroup>("Carton CTN-C");
    carton_c->add(std::make_shared<Item>("line-3", catalog.get("BATTERY-300"), 8, "Austin, TX"));

    auto carton_b = std::make_shared<PackageGroup>("Carton CTN-B");
    carton_b->add(carton_c);

    PackageGroup pallet("Pallet PAL-9");
    pallet.add(carton_a);
    pallet.add(carton_b);

    // carton_a: 40*250 + 12*400 = 14800g, 52 units.
    // carton_c: 8*900 = 7200g, 8 units. carton_b wraps only carton_c, so its
    // totals equal carton_c's, recursed through one extra level of nesting.
    // pallet's totals are carton_a's and carton_b's totals added together.
    EXPECT_EQ(carton_b->total_weight_grams(), 7200);
    EXPECT_EQ(carton_b->item_count(), 8);
    EXPECT_EQ(pallet.total_weight_grams(), 22000);
    EXPECT_EQ(pallet.item_count(), 60);
}

TEST(PackageGroupTest, AnEmptyGroupHasZeroWeightAndZeroItems) {
    PackageGroup carton("Carton CTN-EMPTY");

    EXPECT_EQ(carton.total_weight_grams(), 0);
    EXPECT_EQ(carton.item_count(), 0);
}

TEST(PackageGroupTest, CheckWeightLimitIsANoOpUnderTheLimitEvenWithTheDefaultHandler) {
    SkuCatalog catalog = make_catalog();
    PackageGroup carton("Carton CTN-LIGHT");
    carton.add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 1, "Austin, TX"));

    // 250g, well under any reasonable limit, and no handler was supplied:
    // the call below must still be safe and silent.
    EXPECT_NO_THROW(carton.check_weight_limit(10'000));
}

TEST(PackageGroupTest, CheckWeightLimitReportsToTheBoundHandlerWhenOverLimit) {
    SkuCatalog catalog = make_catalog();
    LoggingShipmentExceptionHandler logger;
    PackageGroup carton("Carton CTN-A", logger);
    carton.add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 40, "Austin, TX"));
    carton.add(std::make_shared<Item>("line-2", catalog.get("GADGET-200"), 12, "Austin, TX"));

    carton.check_weight_limit(500);

    ASSERT_EQ(logger.events().size(), 1);
    EXPECT_EQ(logger.events().front(), "Carton CTN-A is 14300g over its weight limit");
}

TEST(PackageGroupTest, CheckWeightLimitNeverCallsTheHandlerWhenUnderLimit) {
    SkuCatalog catalog = make_catalog();
    LoggingShipmentExceptionHandler logger;
    PackageGroup carton("Carton CTN-LIGHT", logger);
    carton.add(std::make_shared<Item>("line-1", catalog.get("WIDGET-100"), 1, "Austin, TX"));

    carton.check_weight_limit(10'000);

    EXPECT_TRUE(logger.events().empty());
}

}  // namespace
}  // namespace palletizing
