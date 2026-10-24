#include "manifest_serialization.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

#include "manifest_builder.hpp"
#include "package_prototype.hpp"

using manifest::ManifestBuilder;
using manifest::ShippingPriority;

namespace {

manifest::ShipmentManifest sample_manifest() {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-4471")
        .carrier("Northline Freight")
        .destination("Columbus, OH")
        .priority(ShippingPriority::Expedited)
        .insured_value_cents(150000)
        .special_handling_notes("Signature required on delivery")
        .add_package(manifest::make_package("fragile-electronics", 4.50, 120000))
        .add_package(manifest::make_package("bulk-dry-goods", 22.00, 35000));
    return builder.build();
}

}  // namespace

TEST(ManifestSerializationTest, RoundTripReproducesAnEqualManifestWithMultiplePackages) {
    const manifest::ShipmentManifest original = sample_manifest();

    const std::string serialized = manifest::serialize_manifest(original);
    const manifest::ShipmentManifest round_tripped = manifest::deserialize_manifest(serialized);

    EXPECT_TRUE(manifest::manifests_equal(original, round_tripped));
}

TEST(ManifestSerializationTest, RoundTripReproducesAnEqualManifestWithASinglePackageAndDefaults) {
    ManifestBuilder builder;
    builder.shipment_id("SHIP-9001")
        .carrier("Riverside Carriers")
        .destination("Toledo, OH")
        .add_package(manifest::make_package("refrigerated-perishable", 14.75, 60000));
    const manifest::ShipmentManifest original = builder.build();

    const std::string serialized = manifest::serialize_manifest(original);
    const manifest::ShipmentManifest round_tripped = manifest::deserialize_manifest(serialized);

    EXPECT_TRUE(manifest::manifests_equal(original, round_tripped));
    EXPECT_EQ(round_tripped.priority, ShippingPriority::Standard);
    EXPECT_EQ(round_tripped.insured_value_cents, 0);
}

TEST(ManifestSerializationTest, SummaryLineReportsTheExpectedFields) {
    const manifest::ShipmentManifest original = sample_manifest();

    const std::string summary = manifest::summary_line(original);

    EXPECT_NE(summary.find("SHIP-4471"), std::string::npos);
    EXPECT_NE(summary.find("Northline Freight"), std::string::npos);
    EXPECT_NE(summary.find("Columbus, OH"), std::string::npos);
    EXPECT_NE(summary.find("Expedited"), std::string::npos);
    EXPECT_NE(summary.find("2 package(s)"), std::string::npos);
    EXPECT_NE(summary.find("$1500.00"), std::string::npos);
}

TEST(ManifestSerializationTest, DeserializeRejectsAMalformedHeaderLine) {
    const std::string malformed = "SHIPMENT_ID=SHIP-1\nTHIS_LINE_HAS_NO_EQUALS_SIGN\n";

    EXPECT_THROW(manifest::deserialize_manifest(malformed), std::invalid_argument);
}

TEST(ManifestSerializationTest, DeserializeRejectsAnUnrecognizedHeaderKey) {
    const std::string unrecognized = "SHIPMENT_ID=SHIP-1\nNOT_A_REAL_FIELD=value\n";

    EXPECT_THROW(manifest::deserialize_manifest(unrecognized), std::invalid_argument);
}

#if __cplusplus >= 202302L

TEST(ManifestSerializationTest, PrintSummaryMatchesFormatSummaryPlusNewline) {
    const manifest::ShipmentManifest original = sample_manifest();
    const std::string expected = manifest::summary_line(original) + "\n";

    testing::internal::CaptureStdout();
    manifest::print_summary(original);
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(printed, expected);
}

TEST(ManifestSerializationTest, PrintSummaryReflectsFieldsAfterARoundTrip) {
    const manifest::ShipmentManifest original = sample_manifest();
    const std::string serialized = manifest::serialize_manifest(original);
    const manifest::ShipmentManifest round_tripped = manifest::deserialize_manifest(serialized);

    testing::internal::CaptureStdout();
    manifest::print_summary(round_tripped);
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(printed, manifest::summary_line(original) + "\n");
}

#endif
