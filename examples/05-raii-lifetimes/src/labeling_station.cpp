#include "labeling_station.hpp"

#include "dock_bay_lease.hpp"
#include "scope_guard.hpp"

namespace warehouse {

InvalidShipmentError::InvalidShipmentError(const std::string& tracking_id)
    : std::runtime_error("shipment " + tracking_id + " has an invalid barcode") {}

std::size_t label_shipment_batch(DockBayRegistry& registry, std::size_t bay_id, const std::vector<ShipmentItem>& items,
                                  std::size_t& labels_printed) {
    // Acquired here, released automatically on every exit path below:
    // the final return, or the throw inside the loop unwinding the stack
    // straight through this function.
    DockBayLease lease(registry, bay_id);

    labels_printed = 0;

    // Rolls labels_printed back to zero unless the batch reaches
    // dismiss() below. An exception thrown mid-loop skips dismiss(), so
    // the rollback runs during unwinding and the caller's out-parameter
    // never reports a partial count.
    auto rollback_count = on_scope_exit([&labels_printed] { labels_printed = 0; });

    for (const auto& item : items) {
        if (!item.barcode_is_valid) {
            throw InvalidShipmentError(item.tracking_id);
        }
        ++labels_printed;
    }

    rollback_count.dismiss();
    return labels_printed;
}

}  // namespace warehouse
