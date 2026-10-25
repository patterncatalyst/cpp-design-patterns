#include "tracking_store_connection.hpp"

namespace pool24 {

TrackingStoreConnection::TrackingStoreConnection(int connection_id) : connection_id_(connection_id) {}

std::uint64_t TrackingStoreConnection::record_event(std::string_view /*shipment_id*/, std::string_view /*status*/) {
    ++events_recorded_;
    // A real store would return a sequence number it assigned; this stub
    // derives a stand-in from the connection's own running count so each
    // call still returns something distinct and inspectable in a test.
    return static_cast<std::uint64_t>(connection_id_) * 1'000'000ULL + events_recorded_;
}

}  // namespace pool24
