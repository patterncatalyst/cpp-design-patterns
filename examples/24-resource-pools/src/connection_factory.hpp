#pragma once

#include <atomic>
#include <memory>

#include "tracking_store_connection.hpp"

namespace pool24 {

// Seam between TrackingStoreConnectionPool and however a connection
// actually gets built. A test substitutes a mock here to prove the pool
// only calls create() as often as its capacity requires, never once per
// acquire(); production code substitutes a factory that really dials the
// tracking store.
class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    virtual std::unique_ptr<TrackingStoreConnection> create() = 0;
};

// The production factory: hands out connections with ascending ids. Real
// connection setup (DNS, TLS handshake, auth) would live here; this
// chapter only needs the shape of "building one is expensive," which the
// pool amortizes by calling this at most `capacity` times.
class LiveTrackingStoreConnectionFactory : public ConnectionFactory {
public:
    std::unique_ptr<TrackingStoreConnection> create() override;

private:
    std::atomic<int> next_connection_id_{1};
};

}  // namespace pool24
