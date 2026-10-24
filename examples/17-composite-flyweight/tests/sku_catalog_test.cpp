#include "sku_catalog.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace palletizing {
namespace {

SkuCatalog make_catalog() {
    SkuCatalog catalog;
    catalog.register_sku({"WIDGET-100", "Standard widget", 250, 80, 80, 40, HazmatClass::None});
    catalog.register_sku({"GADGET-200", "Precision gadget", 400, 120, 90, 60, HazmatClass::None});
    catalog.register_sku({"BATTERY-300", "Lithium battery pack", 900, 150, 100, 70, HazmatClass::Lithium});
    return catalog;
}

TEST(SkuCatalogTest, GetReturnsTheRegisteredDescriptorFields) {
    SkuCatalog catalog = make_catalog();

    const auto widget = catalog.get("WIDGET-100");

    EXPECT_EQ(widget->sku_code, "WIDGET-100");
    EXPECT_EQ(widget->unit_weight_grams, 250);
    EXPECT_EQ(widget->hazmat_class, HazmatClass::None);
}

TEST(SkuCatalogTest, GetReturnsTheSameSharedInstanceForTheSameSkuCode) {
    SkuCatalog catalog = make_catalog();

    const auto first = catalog.get("WIDGET-100");
    const auto second = catalog.get("WIDGET-100");

    EXPECT_EQ(first.get(), second.get());
}

TEST(SkuCatalogTest, GetThrowsForAnUnregisteredSkuCode) {
    SkuCatalog catalog = make_catalog();

    EXPECT_THROW(catalog.get("CRATE-999"), std::out_of_range);
}

TEST(SkuCatalogTest, PoolSizeStaysBoundedAsManyLineItemsReferenceFewSkus) {
    SkuCatalog catalog = make_catalog();
    EXPECT_EQ(catalog.pool_size(), 0);

    for (int i = 0; i < 100; ++i) {
        const char* sku_code = (i % 3 == 0) ? "WIDGET-100" : (i % 3 == 1) ? "GADGET-200" : "BATTERY-300";
        catalog.get(sku_code);
    }

    // 100 lookups across only 3 distinct SKU codes materialize exactly 3
    // flyweights; the pool's size tracks variety, not call count.
    EXPECT_EQ(catalog.pool_size(), 3);
}

TEST(SkuCatalogTest, PoolSizeGrowsOnlyWithDistinctSkusRequested) {
    SkuCatalog catalog = make_catalog();

    catalog.get("WIDGET-100");
    EXPECT_EQ(catalog.pool_size(), 1);

    catalog.get("WIDGET-100");
    EXPECT_EQ(catalog.pool_size(), 1);

    catalog.get("GADGET-200");
    EXPECT_EQ(catalog.pool_size(), 2);
}

#if __cplusplus >= 202302L
TEST(SkuCatalogTest, TryGetReturnsTheSameInstanceAsGetUnderCpp23) {
    SkuCatalog catalog = make_catalog();

    const auto via_get = catalog.get("GADGET-200");
    const auto via_try_get = catalog.try_get("GADGET-200");

    ASSERT_TRUE(via_try_get.has_value());
    EXPECT_EQ(via_get.get(), via_try_get->get());
}

TEST(SkuCatalogTest, TryGetReturnsATypedErrorForAnUnregisteredSkuUnderCpp23) {
    SkuCatalog catalog = make_catalog();

    const auto result = catalog.try_get("CRATE-999");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "unregistered SKU code: CRATE-999");
}
#endif

}  // namespace
}  // namespace palletizing
