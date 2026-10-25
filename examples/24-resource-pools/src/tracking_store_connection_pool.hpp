#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>

#if __cplusplus >= 202302L
#include <expected>
#endif

#include "connection_factory.hpp"
#include "resource_pool.hpp"
#include "tracking_store_connection.hpp"

namespace pool24 {

// The canonical instance of the generic ResourcePool: a bounded pool of
// connections to the shipment-tracking store. This class adds nothing a
// caller cannot already do with ResourcePool<TrackingStoreConnection>
// directly; it exists so call sites depend on a named, domain-shaped type
// ("a tracking-store connection pool") instead of a bare template
// instantiation, and so the ConnectionFactory seam has one obvious home.
class TrackingStoreConnectionPool {
public:
    using Lease = ResourcePool<TrackingStoreConnection>::Lease;

    TrackingStoreConnectionPool(std::size_t capacity, std::shared_ptr<ConnectionFactory> factory)
        : factory_(std::move(factory)),
          pool_(capacity, [this] { return factory_->create(); }) {}

    Lease acquire(std::chrono::milliseconds timeout) { return pool_.acquire(timeout); }

#if __cplusplus >= 202302L
    std::expected<Lease, PoolAcquireError> try_acquire() { return pool_.try_acquire(); }
#else
    std::optional<Lease> try_acquire() { return pool_.try_acquire(); }
#endif

    void shutdown() { pool_.shutdown(); }

    std::size_t capacity() const { return pool_.capacity(); }
    std::size_t created() const { return pool_.created(); }
    std::size_t available() const { return pool_.available(); }

private:
    std::shared_ptr<ConnectionFactory> factory_;
    ResourcePool<TrackingStoreConnection> pool_;
};

}  // namespace pool24
