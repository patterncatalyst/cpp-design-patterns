#include "manifest_builder.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

#include "package_prototype.hpp"

using manifest::ManifestBuilder;
using manifest::ShippingPriority;

TEST(ManifestBuilderTest, BuildsAValidManifestWithDefaultsForOptionalFields) {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-1001").carrier("Northline Freight").destination("Columbus, OH");
    builder.add_package(manifest::make_package("bulk-dry-goods", 10.0, 20000));

    const manifest::ShipmentManifest built = builder.build();

    EXPECT_EQ(built.shipment_id, "SHIP-1001");
    EXPECT_EQ(built.carrier, "Northline Freight");
    EXPECT_EQ(built.destination, "Columbus, OH");
    EXPECT_EQ(built.priority, ShippingPriority::Standard);
    EXPECT_EQ(built.insured_value_cents, 0);
    EXPECT_EQ(built.special_handling_notes, "");
    ASSERT_EQ(built.packages.size(), 1u);
}

TEST(ManifestBuilderTest, AppliesExplicitOptionalFieldsOverTheirDefaults) {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-1002")
        .carrier("Northline Freight")
        .destination("Dayton, OH")
        .priority(ShippingPriority::Overnight)
        .insured_value_cents(250000)
        .special_handling_notes("Call ahead on arrival")
        .add_package(manifest::make_package("fragile-electronics", 1.5, 90000));

    const manifest::ShipmentManifest built = builder.build();

    EXPECT_EQ(built.priority, ShippingPriority::Overnight);
    EXPECT_EQ(built.insured_value_cents, 250000);
    EXPECT_EQ(built.special_handling_notes, "Call ahead on arrival");
}

TEST(ManifestBuilderTest, ThrowsWhenARequiredFieldIsMissing) {
    ManifestBuilder builder;
    builder.carrier("Northline Freight").destination("Columbus, OH").add_package(manifest::make_package("bulk-dry-goods", 10.0, 20000));

    EXPECT_THROW(builder.build(), std::invalid_argument);
}

TEST(ManifestBuilderTest, ThrowsWhenNoPackageHasBeenAdded) {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-1003").carrier("Northline Freight").destination("Columbus, OH");

    EXPECT_THROW(builder.build(), std::invalid_argument);
}

TEST(ManifestBuilderTest, BuildMovesStateOutLeavingTheBuilderReadyForTheNextManifest) {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-1004").carrier("Northline Freight").destination("Columbus, OH");
    builder.add_package(manifest::make_package("bulk-dry-goods", 5.0, 15000));

    const manifest::ShipmentManifest first = builder.build();
    EXPECT_EQ(first.shipment_id, "SHIP-1004");

    // The builder was reset by the first build(); assembling a second
    // manifest from scratch must not see any leftover state (in
    // particular, an empty package list means this build() must throw).
    EXPECT_THROW(builder.build(), std::invalid_argument);

    builder.shipment_id("SHIP-1005").carrier("Northline Freight").destination("Dayton, OH");
    builder.add_package(manifest::make_package("fragile-electronics", 2.0, 60000));
    const manifest::ShipmentManifest second = builder.build();

    EXPECT_EQ(second.shipment_id, "SHIP-1005");
    ASSERT_EQ(second.packages.size(), 1u);
}
