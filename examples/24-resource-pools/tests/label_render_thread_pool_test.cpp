#include "label_render_thread_pool.hpp"

#include <chrono>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace pool24 {
namespace {

using namespace std::chrono_literals;

TEST(LabelRenderThreadPoolTest, ReportsTheWorkerCountItWasConstructedWith) {
    LabelRenderThreadPool pool(4);
    EXPECT_EQ(pool.worker_count(), 4u);
}

TEST(LabelRenderThreadPoolTest, AllSubmittedTasksRunExactlyOnce) {
    constexpr int kTaskCount = 20;
    LabelRenderThreadPool pool(3);

    std::vector<std::future<RenderedLabel>> futures;
    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.submit(ShippingLabelRequest{
            .shipment_id = "SHM-" + std::to_string(i),
            .destination_city = "Omaha, NE",
            .weight_kg = 3.0,
        }));
    }

    std::set<std::string> rendered_ids;
    for (auto& future : futures) {
        ASSERT_EQ(future.wait_for(2s), std::future_status::ready)
            << "a submitted task must run to completion within a bounded wait";
        rendered_ids.insert(future.get().shipment_id);
    }

    ASSERT_EQ(rendered_ids.size(), static_cast<std::size_t>(kTaskCount))
        << "every submitted shipment id must appear exactly once in the rendered results";
    for (int i = 0; i < kTaskCount; ++i) {
        EXPECT_TRUE(rendered_ids.count("SHM-" + std::to_string(i)) == 1);
    }
}

TEST(LabelRenderThreadPoolTest, RenderedLabelTextIncludesTheRequestFields) {
    LabelRenderThreadPool pool(1);

    auto future = pool.submit(ShippingLabelRequest{
        .shipment_id = "SHM-9001",
        .destination_city = "Topeka, KS",
        .weight_kg = 7.25,
    });

    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    const RenderedLabel label = future.get();
    EXPECT_EQ(label.shipment_id, "SHM-9001");
    EXPECT_NE(label.label_text.find("Topeka, KS"), std::string::npos);
    EXPECT_NE(label.label_text.find("SHM-9001"), std::string::npos);
}

}  // namespace
}  // namespace pool24
