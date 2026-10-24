// Observer and Mediator against a shipping domain: a ShipmentTracker fans
// out status changes to a dashboard, an alerting service, and an audit
// log, and a DispatchCoordinator routes events between a warehouse, a
// carrier, and a customer-notification station that never reference each
// other directly.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <string>
#include <vector>

#include "dispatch_mediator.hpp"
#include "shipment_observer.hpp"

namespace {

void print_section(const char* title) {
    std::cout << "\n== " << title << " ==\n";
}

}  // namespace

int main() {
    using namespace shipping;

    print_section("Observer: interface-based subscribers fan out on a status change");
    {
        ShipmentTracker tracker("TRK-7001");
        DashboardView dashboard;
        AlertingService alerting;

        auto dashboard_sub = tracker.subscribe(dashboard);
        auto alerting_sub = tracker.subscribe(alerting);

        tracker.advance_status(ShipmentStatus::PickedUp);
        tracker.advance_status(ShipmentStatus::InTransit);
        std::cout << "Dashboard now reads: " << dashboard.current_status_line() << '\n';

        tracker.advance_status(ShipmentStatus::Exception);
        std::cout << "Dashboard now reads: " << dashboard.current_status_line() << '\n';
        std::cout << "Alerts raised: " << alerting.alerts().size() << '\n';
        for (const auto& alert : alerting.alerts()) {
            std::cout << "  " << alert << '\n';
        }
    }

    print_section("Observer: a std::function callback needs no observer class at all");
    {
        ShipmentTracker tracker("TRK-7002");
        std::vector<std::string> audit_entries;
        auto audit_sub = tracker.subscribe(
            [&audit_entries](const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status) {
                audit_entries.push_back(tracking_id + ": " + to_string(old_status) + " -> " + to_string(new_status));
            });

        tracker.advance_status(ShipmentStatus::PickedUp);
        tracker.advance_status(ShipmentStatus::InTransit);
        tracker.advance_status(ShipmentStatus::Delivered);

        for (const auto& entry : audit_entries) {
            std::cout << entry << '\n';
        }
    }

    print_section("Observer: unsubscribing stops notification, even mid-session");
    {
        ShipmentTracker tracker("TRK-7003");
        int call_count = 0;
        {
            auto sub = tracker.subscribe(
                [&call_count](const std::string&, ShipmentStatus, ShipmentStatus) { ++call_count; });
            tracker.advance_status(ShipmentStatus::PickedUp);
        }
        tracker.advance_status(ShipmentStatus::InTransit);
        std::cout << "Callback ran " << call_count << " time(s); subscriber_count() is now "
                   << tracker.subscriber_count() << '\n';
    }

    print_section("Mediator: a warehouse event cascades through the coordinator");
    {
        WarehouseStation warehouse;
        CarrierStation carrier;
        CustomerNotificationStation notifier;
        DispatchCoordinator coordinator(warehouse, carrier, notifier);

        warehouse.mark_order_ready("ORD-5001");
        carrier.mark_out_for_delivery("ORD-5001");
        carrier.report_exception("ORD-5001", "damaged during transit");

        std::cout << "Carrier's own record of pickups: " << carrier.picked_up_orders().size() << '\n';
        std::cout << "Messages the coordinator routed to the customer:\n";
        for (const auto& message : notifier.sent_messages()) {
            std::cout << "  " << message << '\n';
        }
    }

#if __cplusplus >= 202302L
    print_section("C++23 refinement: typed rejection of a subscribe after a terminal status");
    {
        ShipmentTracker tracker("TRK-7004");
        tracker.advance_status(ShipmentStatus::Delivered);
        DashboardView dashboard;
        const auto result = tracker.try_subscribe(
            [&dashboard](const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status) {
                dashboard.on_status_changed(tracking_id, old_status, new_status);
            });
        if (!result.has_value()) {
            std::cout << "Rejected subscribe: " << result.error() << '\n';
        }
    }
#endif

    return 0;
}
