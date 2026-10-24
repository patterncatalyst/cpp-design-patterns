#include "package_manifest.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

namespace shipping {
namespace {

PackageManifest sample_manifest() {
    PackageManifest manifest;
    manifest.add(Package{"TRK-INTL-1", 2.0, DestinationZone::International});
    manifest.add(Package{"TRK-LOC-1", 1.0, DestinationZone::Local});
    manifest.add(Package{"TRK-NAT-1", 5.0, DestinationZone::National});
    manifest.add(Package{"TRK-NAT-2", 7.5, DestinationZone::National});
    manifest.add(Package{"TRK-REG-1", 3.0, DestinationZone::Regional});
    return manifest;
}

TEST(PackageManifestTest, CustomIteratorTraversesInZoneOrderRegardlessOfInsertionOrder) {
    const PackageManifest manifest = sample_manifest();

    std::vector<std::string> tracking_numbers;
    for (const Package& package : manifest) {
        tracking_numbers.push_back(package.tracking_number);
    }

    EXPECT_EQ(tracking_numbers,
              (std::vector<std::string>{"TRK-LOC-1", "TRK-REG-1", "TRK-NAT-1", "TRK-NAT-2", "TRK-INTL-1"}));
}

TEST(PackageManifestTest, InteroperatesWithStandardAlgorithms) {
    const PackageManifest manifest = sample_manifest();

    const auto found =
        std::find_if(manifest.begin(), manifest.end(),
                      [](const Package& package) { return package.zone == DestinationZone::Regional; });

    ASSERT_NE(found, manifest.end());
    EXPECT_EQ(found->tracking_number, "TRK-REG-1");
}

TEST(PackageManifestTest, EmptyManifestBeginEqualsEnd) {
    const PackageManifest manifest;

    EXPECT_EQ(manifest.begin(), manifest.end());
    EXPECT_EQ(manifest.size(), 0u);
}

TEST(PackageManifestTest, RangesPipelineFiltersAndTransformsTheSameUnderlyingData) {
    const PackageManifest manifest = sample_manifest();

    auto national_weights =
        manifest | std::views::filter([](const Package& package) { return package.zone == DestinationZone::National; }) |
        std::views::transform([](const Package& package) { return package.weight_kg; });

    const std::vector<double> weights(national_weights.begin(), national_weights.end());
    EXPECT_EQ(weights, (std::vector<double>{5.0, 7.5}));
}

#if __cplusplus >= 202302L
TEST(PackageManifestTest, RangesToCollectsTheFilteredAndTransformedPipeline) {
    const PackageManifest manifest = sample_manifest();

    auto national_tracking_numbers =
        manifest | std::views::filter([](const Package& package) { return package.zone == DestinationZone::National; }) |
        std::views::transform([](const Package& package) { return package.tracking_number; }) |
        std::ranges::to<std::vector>();

    EXPECT_EQ(national_tracking_numbers, (std::vector<std::string>{"TRK-NAT-1", "TRK-NAT-2"}));
}
#endif

}  // namespace
}  // namespace shipping
