#include "manifest_source.hpp"

namespace breakdep28 {

DispatchSummary WarehouseDispatcher::dispatch(const ManifestSource& manifest) const {
    DispatchSummary summary;
    summary.carrier_code = manifest.carrier_code();
    summary.shipment_count = manifest.shipment_count();
    summary.large_batch = manifest.shipment_count() >= large_batch_threshold_;
    return summary;
}

}  // namespace breakdep28
