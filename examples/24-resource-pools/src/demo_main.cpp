// Exercises all five resource-pool-shaped patterns this chapter covers
// against a shipment-tracking domain: the generic ResourcePool + Lease, a
// TrackingStoreConnectionPool built on it, a LabelRenderThreadPool, a
// ShipmentManifestMonitor, and a LabelPrinterActiveObject.

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "connection_factory.hpp"
#include "label_printer_active_object.hpp"
#include "label_render_thread_pool.hpp"
#include "resource_pool.hpp"
#include "shipment_manifest_monitor.hpp"
#include "tracking_store_connection_pool.hpp"

using namespace std::chrono_literals;
using pool24::LabelPrinterActiveObject;
using pool24::LabelRenderThreadPool;
using pool24::LiveTrackingStoreConnectionFactory;
using pool24::ShipmentManifestMonitor;
using pool24::ShippingLabelRequest;
using pool24::TrackingStoreConnectionPool;

namespace {

void demo_connection_pool() {
    std::cout << "== Connection pool: bounded, blocking, reused ==\n";

    auto factory = std::make_shared<LiveTrackingStoreConnectionFactory>();
    TrackingStoreConnectionPool pool(2, factory);

    auto lease_a = pool.acquire(100ms);
    auto lease_b = pool.acquire(100ms);
    std::cout << "Leased connections " << lease_a->connection_id() << " and " << lease_b->connection_id()
              << "; pool created=" << pool.created() << " of capacity=" << pool.capacity() << "\n";

#if __cplusplus >= 202302L
    auto third = pool.try_acquire();
    if (!third.has_value()) {
        std::cout << "try_acquire while exhausted: " << pool24::describe(third.error()) << "\n";
    }
#else
    auto third = pool.try_acquire();
    std::cout << "try_acquire while exhausted: " << (third.has_value() ? "unexpectedly succeeded" : "failed (no reason available)")
              << "\n";
#endif

    lease_a->record_event("SHM-1001", "in_transit");
    const int reused_id = lease_a->connection_id();
    lease_a = TrackingStoreConnectionPool::Lease{};  // release early: returns to the free list

    auto lease_c = pool.acquire(100ms);
    std::cout << "After releasing one lease, acquire() reused connection " << lease_c->connection_id()
              << " (same id as before: " << (lease_c->connection_id() == reused_id ? "yes" : "no")
              << "); pool still created only " << pool.created() << " connection(s) total\n";
    std::cout << "\n";
}

void demo_thread_pool() {
    std::cout << "== Thread pool: workers draining a shipping-label task queue ==\n";

    LabelRenderThreadPool pool(3);
    std::vector<std::future<pool24::RenderedLabel>> futures;
    for (int i = 1; i <= 5; ++i) {
        futures.push_back(pool.submit(ShippingLabelRequest{
            .shipment_id = "SHM-" + std::to_string(2000 + i),
            .destination_city = "Columbus, OH",
            .weight_kg = 4.5 + i,
        }));
    }

    for (auto& future : futures) {
        auto label = future.get();
        std::cout << "Rendered label for " << label.shipment_id << "\n";
    }
    std::cout << "All " << futures.size() << " submitted labels rendered by " << pool.worker_count()
              << " worker thread(s)\n\n";
}

void demo_monitor_object() {
    std::cout << "== Monitor object: ShipmentManifestMonitor serializes concurrent scans ==\n";

    ShipmentManifestMonitor manifest("SHM-3001");
    std::vector<std::jthread> scanners;
    for (int i = 0; i < 4; ++i) {
        scanners.emplace_back([&manifest, i] {
            for (int j = 0; j < 25; ++j) {
                manifest.add_item("SKU-" + std::to_string(i * 100 + j), 1.0);
            }
        });
    }
    scanners.clear();  // jthread destructors join

    std::cout << "4 concurrent scanners each added 25 items; manifest now holds " << manifest.item_count()
              << " items, total weight " << manifest.total_weight_kg() << " kg\n\n";
}

void demo_active_object() {
    std::cout << "== Active object: LabelPrinterActiveObject decouples call from execution ==\n";

    LabelPrinterActiveObject printer;
    const auto caller_thread = std::this_thread::get_id();

    std::vector<std::future<pool24::PrintResult>> futures;
    for (int i = 1; i <= 3; ++i) {
        futures.push_back(printer.print_label(ShippingLabelRequest{
            .shipment_id = "SHM-" + std::to_string(4000 + i),
            .destination_city = "Reno, NV",
            .weight_kg = 2.0 * i,
        }));
    }

    for (auto& future : futures) {
        auto result = future.get();
        std::cout << "Printed " << result.label.shipment_id << " on "
                  << (result.executed_on == caller_thread ? "the caller's thread" : "a different thread") << "\n";
    }
    std::cout << "\n";
}

}  // namespace

int main() {
    demo_connection_pool();
    demo_thread_pool();
    demo_monitor_object();
    demo_active_object();
    return 0;
}
