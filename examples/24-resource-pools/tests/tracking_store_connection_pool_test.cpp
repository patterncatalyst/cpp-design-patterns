#include "tracking_store_connection_pool.hpp"

#include <chrono>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pool24 {
namespace {

using namespace std::chrono_literals;
using ::testing::Invoke;

class MockConnectionFactory : public ConnectionFactory {
public:
    MOCK_METHOD(std::unique_ptr<TrackingStoreConnection>, create, (), (override));
};

TEST(TrackingStoreConnectionPoolTest, CreatesAtMostCapacityConnectionsAcrossManyAcquireReleaseCycles) {
    auto factory = std::make_shared<MockConnectionFactory>();
    int next_id = 1;
    // The pool's capacity is 2. Ten acquire/release cycles below must still
    // only ever call create() twice: once to build each of the two
    // connections the pool is allowed, never again after that.
    EXPECT_CALL(*factory, create()).Times(2).WillRepeatedly(Invoke([&next_id] {
        return std::make_unique<TrackingStoreConnection>(next_id++);
    }));

    TrackingStoreConnectionPool pool(2, factory);

    for (int i = 0; i < 10; ++i) {
        auto lease_a = pool.acquire(100ms);
        auto lease_b = pool.acquire(100ms);
        // Both leases release when they go out of scope at the end of
        // this iteration, returning both connections to the free list.
    }

    EXPECT_EQ(pool.created(), 2u);
}

TEST(TrackingStoreConnectionPoolTest, LeaseReturnsItsConnectionToThePoolOnDestruction) {
    auto factory = std::make_shared<MockConnectionFactory>();
    int next_id = 1;
    EXPECT_CALL(*factory, create()).WillRepeatedly(Invoke([&next_id] {
        return std::make_unique<TrackingStoreConnection>(next_id++);
    }));

    TrackingStoreConnectionPool pool(1, factory);

    int first_connection_id = 0;
    {
        auto lease = pool.acquire(100ms);
        first_connection_id = lease->connection_id();
        lease->record_event("SHM-5001", "picked_up");
    }

    auto lease_again = pool.acquire(100ms);
    EXPECT_EQ(lease_again->connection_id(), first_connection_id)
        << "the pool must hand back the same connection, not build a new one";
    EXPECT_EQ(pool.created(), 1u);
}

TEST(TrackingStoreConnectionPoolTest, AcquireTimesOutWhenCapacityIsExhausted) {
    auto factory = std::make_shared<MockConnectionFactory>();
    int next_id = 1;
    EXPECT_CALL(*factory, create()).Times(1).WillOnce(Invoke([&next_id] {
        return std::make_unique<TrackingStoreConnection>(next_id++);
    }));

    TrackingStoreConnectionPool pool(1, factory);
    auto lease = pool.acquire(100ms);

    EXPECT_THROW(pool.acquire(20ms), std::runtime_error)
        << "a capacity-1 pool with its only connection leased must time out, not create a second connection";
}

}  // namespace
}  // namespace pool24
