#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "dock_bay_registry.hpp"

namespace warehouse {

struct ShipmentItem {
    std::string tracking_id;
    bool barcode_is_valid;
};

// Thrown when a shipment item's barcode fails validation partway through
// a batch.
class InvalidShipmentError : public std::runtime_error {
public:
    explicit InvalidShipmentError(const std::string& tracking_id);
};

// Labels every item in a batch at one dock bay. The bay is leased for the
// whole batch and released no matter how this function returns: a normal
// return after the last item, or an exception thrown when an item's
// barcode fails validation partway through. labels_printed is updated as
// each item is labeled and rolled back to zero if the batch throws, so a
// caller catching the exception never sees a partial count left over from
// a failed batch, only the all-or-nothing outcome the batch actually
// reached. Returns the count on success.
std::size_t label_shipment_batch(DockBayRegistry& registry, std::size_t bay_id, const std::vector<ShipmentItem>& items,
                                  std::size_t& labels_printed);

}  // namespace warehouse
