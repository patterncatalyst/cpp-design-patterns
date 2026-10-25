#include "connection_factory.hpp"

namespace pool24 {

std::unique_ptr<TrackingStoreConnection> LiveTrackingStoreConnectionFactory::create() {
    return std::make_unique<TrackingStoreConnection>(next_connection_id_.fetch_add(1));
}

}  // namespace pool24
