#include "resource_pool.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

namespace pool24 {
namespace {

using namespace std::chrono_literals;

// A minimal stand-in resource: just an id and a counter of how many times
// one has ever been built, so a test can prove the pool reuses an
// instance instead of building a fresh one on every acquire.
struct Widget {
    explicit Widget(int widget_id) : id(widget_id) { ++live_construction_count; }
    int id;
    static inline int live_construction_count = 0;
};

ResourcePool<Widget> make_pool(std::size_t capacity) {
    // Captured by value: the lambda outlives this function, so it must not
    // hold a reference to a local that is about to go out of scope.
    return ResourcePool<Widget>(capacity, [next_id = 1]() mutable { return std::make_unique<Widget>(next_id++); });
}

TEST(ResourcePoolTest, AcquireBuildsLazilyUpToCapacity) {
    Widget::live_construction_count = 0;
    auto pool = make_pool(2);

    EXPECT_EQ(pool.created(), 0u);
    auto lease_a = pool.acquire(50ms);
    EXPECT_EQ(pool.created(), 1u);
    auto lease_b = pool.acquire(50ms);
    EXPECT_EQ(pool.created(), 2u);
    EXPECT_EQ(Widget::live_construction_count, 2);
}

TEST(ResourcePoolTest, LeaseReturnsItsResourceToThePoolOnDestruction) {
    auto pool = make_pool(1);

    int first_id = 0;
    {
        auto lease = pool.acquire(50ms);
        first_id = lease->id;
    }  // lease destructor runs here, must return the Widget to the free list

    EXPECT_EQ(pool.available(), 1u);

    auto lease_again = pool.acquire(50ms);
    EXPECT_EQ(lease_again->id, first_id) << "acquire() after release should hand back the SAME widget, not build a new one";
    EXPECT_EQ(pool.created(), 1u) << "the pool must never construct a second widget when the first one was released";
}

TEST(ResourcePoolTest, TryAcquireFailsWithoutBlockingWhenExhausted) {
    auto pool = make_pool(1);
    auto lease = pool.acquire(50ms);

    auto result = pool.try_acquire();

    EXPECT_FALSE(result.has_value()) << "every widget is leased; try_acquire() must not block or succeed";
}

TEST(ResourcePoolTest, ExhaustionBlocksAnAcquireUntilARelease) {
    auto pool = make_pool(1);
    auto lease = pool.acquire(50ms);

    std::promise<int> acquired_id;
    std::jthread waiter([&pool, &acquired_id] {
        auto waiting_lease = pool.acquire(2s);
        acquired_id.set_value(waiting_lease->id);
    });
    std::future<int> acquired_id_future = acquired_id.get_future();

    // The waiter must still be blocked a short while after it started:
    // the only widget in this capacity-1 pool is still leased by `lease`.
    ASSERT_EQ(acquired_id_future.wait_for(50ms), std::future_status::timeout)
        << "acquire() must block while the pool is exhausted, not return early";

    const int held_id = lease->id;
    lease = ResourcePool<Widget>::Lease{};  // release: returns the widget and wakes the waiter

    ASSERT_EQ(acquired_id_future.wait_for(2s), std::future_status::ready)
        << "releasing the only lease must wake the blocked acquire()";
    EXPECT_EQ(acquired_id_future.get(), held_id) << "the unblocked acquire() must receive the just-released widget";
}

TEST(ResourcePoolTest, AcquireTimesOutWhenNothingIsEverReleased) {
    auto pool = make_pool(1);
    auto lease = pool.acquire(50ms);  // never released within this test

    EXPECT_THROW(pool.acquire(20ms), std::runtime_error);
}

TEST(ResourcePoolTest, ShutdownWakesABlockedAcquireInsteadOfMakingItWaitOutItsTimeout) {
    auto pool = make_pool(1);
    auto lease = pool.acquire(50ms);

    std::promise<void> waiter_done;
    std::jthread waiter([&pool, &waiter_done] {
        EXPECT_THROW(pool.acquire(10s), std::runtime_error);
        waiter_done.set_value();
    });
    std::future<void> waiter_done_future = waiter_done.get_future();

    pool.shutdown();

    ASSERT_EQ(waiter_done_future.wait_for(2s), std::future_status::ready)
        << "shutdown() must wake a blocked acquire() long before its 10-second timeout elapses";
}

#if __cplusplus >= 202302L
TEST(ResourcePoolTest, TryAcquireReportsWhyItFailedUnderCpp23) {
    auto pool = make_pool(1);
    auto lease = pool.acquire(50ms);

    auto exhausted = pool.try_acquire();
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(exhausted.error(), PoolAcquireError::Exhausted);

    lease = ResourcePool<Widget>::Lease{};
    pool.shutdown();

    auto closed = pool.try_acquire();
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error(), PoolAcquireError::Closed)
        << "a closed pool must report Closed, distinct from Exhausted, which a C++20 optional cannot express";
}
#endif

}  // namespace
}  // namespace pool24
