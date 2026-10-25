#include "manifest_source.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace breakdep28 {
namespace {

class FakeManifestSource : public ManifestSource {
public:
    FakeManifestSource(std::string carrier_code, int shipment_count)
        : carrier_code_(std::move(carrier_code)), shipment_count_(shipment_count) {}

    int shipment_count() const override { return shipment_count_; }
    std::string carrier_code() const override { return carrier_code_; }

private:
    std::string carrier_code_;
    int shipment_count_;
};

TEST(WarehouseDispatcherTest, FlagsALargeBatchWithNoFileOnDisk) {
    // Adapt Parameter's whole claim: this test never touches a filesystem.
    FakeManifestSource manifest("FASTFREIGHT", 250);
    WarehouseDispatcher dispatcher(200);

    const auto summary = dispatcher.dispatch(manifest);

    EXPECT_EQ(summary.carrier_code, "FASTFREIGHT");
    EXPECT_EQ(summary.shipment_count, 250);
    EXPECT_TRUE(summary.large_batch);
}

TEST(WarehouseDispatcherTest, SmallBatchIsNotFlagged) {
    FakeManifestSource manifest("REGIONALPARCEL", 12);
    WarehouseDispatcher dispatcher(200);

    EXPECT_FALSE(dispatcher.dispatch(manifest).large_batch);
}

TEST(WarehouseDispatcherTest, AdapterWrapsARealManifestFileIdentically) {
    const auto temp_path = std::filesystem::temp_directory_path() / "breakdep28_manifest_test.txt";
    {
        std::ofstream out(temp_path);
        out << "FASTFREIGHT\n250\n";
    }

    const CarrierManifestFile file(temp_path.string());
    const CarrierManifestFileAdapter adapter(file);
    WarehouseDispatcher dispatcher(200);

    const auto summary = dispatcher.dispatch(adapter);

    EXPECT_EQ(summary.carrier_code, "FASTFREIGHT");
    EXPECT_EQ(summary.shipment_count, 250);
    EXPECT_TRUE(summary.large_batch);

    std::filesystem::remove(temp_path);
}

}  // namespace
}  // namespace breakdep28
