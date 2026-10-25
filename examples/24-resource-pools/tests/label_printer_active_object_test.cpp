#include "label_printer_active_object.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace pool24 {
namespace {

using namespace std::chrono_literals;

TEST(LabelPrinterActiveObjectTest, PrintLabelRunsOnADifferentThreadThanTheCaller) {
    LabelPrinterActiveObject printer;
    const auto caller_thread = std::this_thread::get_id();

    auto future = printer.print_label(ShippingLabelRequest{
        .shipment_id = "SHM-7001",
        .destination_city = "Boise, ID",
        .weight_kg = 1.5,
    });

    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    const PrintResult result = future.get();
    EXPECT_NE(result.executed_on, caller_thread)
        << "an active object must run the request on its own worker thread, not the caller's";
    EXPECT_EQ(result.label.shipment_id, "SHM-7001");
}

TEST(LabelPrinterActiveObjectTest, AllSubmittedRequestsResolveWithTheirOwnLabel) {
    constexpr int kRequestCount = 10;
    LabelPrinterActiveObject printer;

    std::vector<std::future<PrintResult>> futures;
    for (int i = 0; i < kRequestCount; ++i) {
        futures.push_back(printer.print_label(ShippingLabelRequest{
            .shipment_id = "SHM-" + std::to_string(8000 + i),
            .destination_city = "Eugene, OR",
            .weight_kg = 2.0,
        }));
    }

    for (int i = 0; i < kRequestCount; ++i) {
        ASSERT_EQ(futures[i].wait_for(2s), std::future_status::ready)
            << "every submitted request must resolve within a bounded wait";
        const PrintResult result = futures[i].get();
        EXPECT_EQ(result.label.shipment_id, "SHM-" + std::to_string(8000 + i));
    }
}

TEST(LabelPrinterActiveObjectTest, SubmittingDoesNotBlockTheCallingThread) {
    LabelPrinterActiveObject printer;

    const auto start = std::chrono::steady_clock::now();
    auto future = printer.print_label(ShippingLabelRequest{
        .shipment_id = "SHM-9101",
        .destination_city = "Helena, MT",
        .weight_kg = 9.0,
    });
    const auto submit_duration = std::chrono::steady_clock::now() - start;

    EXPECT_LT(submit_duration, 50ms) << "print_label() must only enqueue the request, never wait for it to run";
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
}

}  // namespace
}  // namespace pool24
