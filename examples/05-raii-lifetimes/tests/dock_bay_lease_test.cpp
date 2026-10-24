#include "dock_bay_lease.hpp"

#include <gtest/gtest.h>

#include <utility>

#include "dock_bay_registry.hpp"

namespace warehouse {
namespace {

TEST(DockBayLeaseTest, AcquiresOnConstructionAndReleasesAtScopeExit) {
    DockBayRegistry registry(2);
    {
        DockBayLease lease(registry, 0);
        EXPECT_TRUE(registry.is_leased(0));
        EXPECT_EQ(lease.bay_id(), 0u);
        EXPECT_TRUE(lease.owns_bay());
    }  // lease destructs here with no explicit release() call

    EXPECT_FALSE(registry.is_leased(0));
    EXPECT_EQ(registry.release_count(), 1u);
}

TEST(DockBayLeaseTest, ThrowsWhenTheBayIsAlreadyLeased) {
    DockBayRegistry registry(2);
    DockBayLease first(registry, 1);

    EXPECT_THROW(DockBayLease(registry, 1), BayUnavailableError);

    // The failed second attempt never took the bay; it is still held by
    // the first lease alone.
    EXPECT_TRUE(registry.is_leased(1));
    EXPECT_EQ(registry.release_count(), 0u);
}

TEST(DockBayLeaseTest, ReleasesTheBayWhenAnExceptionUnwindsThePossessingScope) {
    DockBayRegistry registry(2);

    auto lease_then_throw = [&registry] {
        DockBayLease lease(registry, 0);
        (void)lease;
        throw std::runtime_error("something downstream of the lease failed");
    };

    EXPECT_THROW(lease_then_throw(), std::runtime_error);

    // The lease's destructor ran during stack unwinding even though the
    // function never reached a normal return.
    EXPECT_FALSE(registry.is_leased(0));
    EXPECT_EQ(registry.release_count(), 1u);
}

TEST(DockBayLeaseTest, MoveConstructionTransfersOwnershipWithoutDoubleRelease) {
    DockBayRegistry registry(2);
    {
        DockBayLease original(registry, 0);
        DockBayLease moved(std::move(original));

        EXPECT_FALSE(original.owns_bay());
        EXPECT_TRUE(moved.owns_bay());
        EXPECT_EQ(moved.bay_id(), 0u);
        EXPECT_TRUE(registry.is_leased(0));
    }  // original destructs (no-op) then moved destructs (releases)

    EXPECT_FALSE(registry.is_leased(0));
    // Exactly one genuine release reached the registry, even though two
    // DockBayLease destructors ran on the same bay_id.
    EXPECT_EQ(registry.release_count(), 1u);
}

TEST(DockBayLeaseTest, MoveAssignmentReleasesTheTargetsPriorBayBeforeAdoptingTheNewOne) {
    DockBayRegistry registry(2);
    DockBayLease holding_bay_zero(registry, 0);
    DockBayLease holding_bay_one(registry, 1);

    holding_bay_one = std::move(holding_bay_zero);

    // Bay 1, which holding_bay_one owned before the assignment, released
    // immediately as part of the move assignment, not later.
    EXPECT_FALSE(registry.is_leased(1));
    EXPECT_TRUE(registry.is_leased(0));
    EXPECT_EQ(holding_bay_one.bay_id(), 0u);
    EXPECT_FALSE(holding_bay_zero.owns_bay());
    EXPECT_EQ(registry.release_count(), 1u);
}

#if __cplusplus >= 202302L
TEST(DockBayLeaseTest, TryAcquireRejectsAnAlreadyLeasedBayWithoutThrowing) {
    DockBayRegistry registry(2);
    DockBayLease holder(registry, 0);

    const auto result = DockBayLease::try_acquire(registry, 0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "dock bay 0 is already leased");
}

TEST(DockBayLeaseTest, TryAcquireReturnsAWorkingLeaseForAFreeBay) {
    DockBayRegistry registry(2);

    {
        auto result = DockBayLease::try_acquire(registry, 0);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(registry.is_leased(0));
        EXPECT_EQ(result->bay_id(), 0u);
    }  // the expected, and the DockBayLease it holds, destruct here

    EXPECT_FALSE(registry.is_leased(0));
}
#endif

}  // namespace
}  // namespace warehouse
